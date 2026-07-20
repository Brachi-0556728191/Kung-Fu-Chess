# PLAN.md — Kung Fu Chess External Server

**Read this file at the start of every session before doing anything.**
Work ONLY on the current open stage (the first unchecked box below). Do not start a later stage, do not "improve" an earlier one, unless explicitly asked. When a stage's completion criterion is verified working, check its box, stop, and wait for confirmation before moving to the next stage.

---

## Fixed architecture decisions (do not re-litigate these mid-implementation)

These were settled after review; they are constraints, not suggestions.

### Concurrency model
- "Real-time, no-turns" is a property of the **game rules** (already solved by `RealTimeArbiter::activeMoves_`) — separate from **thread-safety**, which the server introduces.
- **One `std::mutex` per `GameInstance`.** Acquired for exactly one discrete operation each time:
  - the tick thread wraps each `handleWait` call;
  - the network thread wraps each server-side `Controller::click` / `Controller::jump` call;
  - snapshot building acquires it while reading.
- Critical sections are single function calls (microseconds, no I/O) — this does NOT make the game turn-based or add perceptible delay.
- **Per-instance, not global**, from S4 onward (even with one game) — so S9 (multi-game) needs no rework.
- Two more independent locks, never conflated with the above or each other:
  - `MatchmakingPool` (S7) — its own mutex.
  - `RoomManager` (S8) — its own mutex.
- SQLite (S5): amalgamation build defaults to Serialized threading mode — no app-level mutex needed. Confirm the connection is opened in serialized mode; don't hold a transaction open across anything slow.
- **Total: 4 independent locks** (per-GameInstance, matchmaking, rooms, SQLite's own internal one). Never one big lock, never zero locks.

### Snapshot generation
- Do **NOT** add new getters to `RealTimeArbiter`. Build snapshots using only the existing public API: `activeMoveFor(Position)`, `activeRest(pieceId)`, `activeJump()`.
- Snapshot carries **raw** `PieceMove`/`PieceRest`/`PieceJump` data (from/to/startMs/durationMs, startMs/untilMs) — not a pre-resolved discrete frame — because the client needs to keep animating between snapshots off its own clock.
- Snapshot's `moveHistory` field carries only **new entries since the last snapshot** (delta), not the full accumulated list.

### Controller / input handling (decisions #3 + #10 reconciled)
- `Controller::click` / `Controller::jump` **never change** — they keep taking raw `(int x, int y)` pixels and calling `pixelToCell` internally.
- The client resolves pixel→cell **locally** (its own in-memory canvas coords, immune to OS/DPI scaling) and sends the wire message as `(row, col)` — never raw pixels.
- The server-side message handler (new glue code, NOT inside `Controller.cpp`) reconstructs a canonical pixel (e.g. cell center) from `(row, col)` using the shared `config::CELL_SIZE` / `config::HISTORY_PANEL_WIDTH` constants, then calls the untouched `Controller::click`.
- Only **two** message types mirror the two real entry points: `ClickMsg{row,col}` and `JumpMsg{row,col}`. No separate select/move message — the server's existing selection-state logic already resolves that ambiguity; don't duplicate it client-side.

---

## Stage checklist

- [x] **S0 — AnimationState.cpp decoupling refactor** *(local only, no networking)*
  Change `resolveAnimationState` and `restRemainingFraction` to accept raw `optional<PieceMove>` / `optional<PieceJump>` / `optional<PieceRest>` directly, instead of `const RealTimeArbiter&`.
  **Done when:** existing test suite passes + visual render check shows local single-player rendering is pixel-identical to before.

- [ ] **S1 — Basic WebSocket connection (echo)**
  `server_echo.cpp` opens `ix::WebSocketServer` on a fixed port, echoes every message back. `client_echo.cpp` connects, sends `"ping"`, prints the reply.
  **Done when:** two terminals, round-trip text message works.

- [ ] **S2 — Message protocol design**
  Write `PROTOCOL.md` + `messages.h` (structs only, no JSON yet).
  - Client→Server: `login`, `play`, `room_join`, `room_create`, `ClickMsg{row,col}`, `JumpMsg{row,col}`
  - Server→Client: `login_ok`/`login_fail`, `match_found`/`no_match_found`, `spectating`, `snapshot`, `opponent_disconnected`, `game_over`
  - `snapshot` fields: board contents, per-piece raw move/rest/jump data, score, `game_over` state, delta `moveHistory` only.
  **Done when:** doc + structs exist and are internally consistent (not yet wired to code).

- [ ] **S3 — JSON serialization over echo**
  Add `to_json`/`from_json` for every struct from S2. Upgrade S1's echo pair to send/receive real structs as JSON instead of `"ping"`.
  **Done when:** round-trip struct → JSON → struct is identical to the original, over the real network.

- [ ] **S4 — Wire up the real GameEngine, one global game, end-to-end**
  - Split `main.cpp` into `server_main.cpp` (GameState + RealTimeArbiter + GameEngine + Controller, no view/OpenCV, own tick loop under the per-instance mutex) and `client_main.cpp` (view::renderBoard + new independent input handler using `BoardMapper::pixelToCell` + WebSocket client, no local Controller/GameEngine).
  - Client also links `rules/RuleEngine.cpp` + `rules/PieceRules.cpp` locally for **cosmetic-only** legal-move highlight preview (server always re-validates independently — a stale client highlight has no correctness/security impact).
  - Server message handler reconstructs canonical pixel from `(row,col)` and calls unmodified `Controller::click`/`jump`.
  - Snapshot broadcast built under the per-instance mutex using S0-refactored functions.
  - Color assignment: first-connect = white, second = black (global variable for now).
  **Done when:** 2 client windows open, clicking a piece in one moves it in both, full rule logic (legality, cooldown, capture) works over the network.

- [ ] **S5 — Login + SQLite**
  `users(username TEXT PRIMARY KEY, password_hash TEXT, elo INTEGER DEFAULT 1000)`. Password always hashed, never plaintext. `LoginMsg` → `login_ok{elo}` / `login_fail{reason}`.
  **Done when:** same username logging in twice returns the same persisted elo both times.

- [ ] **S6 — ELO calculation and update**
  `expected = 1 / (1 + 10^((elo_opponent - elo_self)/400))`, `new_elo = elo_self + K*(actual_score - expected)`, K=32. Triggered on real `game_over` from GameEngine, updates both DB records.
  **Done when:** game between differently-rated users completes; winner beating a stronger opponent gains more points than beating an equal one.

- [ ] **S7 — Matchmaking by ELO range (Play)**
  Range **±200**, wait window **~30 seconds** (updated from the original ±100 instruction — deliberate design decision). No range widening. Uses the `MatchmakingPool` mutex.
  **Done when:** two in-range users get matched; an out-of-range user gets `no_match_found` after the window.

- [ ] **S8 — Rooms (first = opponent, rest = spectators)**
  `RoomManager` under its own mutex. Server-side spectator enforcement: every incoming `ClickMsg`/`JumpMsg` checked against the room's registered opponent connections before reaching `Controller::click/jump` — rejected server-side regardless of client UI. Room creation/joining does not require login.
  **Done when:** 3 clients join the same room name — first 2 play, 3rd only observes (server rejects any click attempt from it).
  ⚠️ **Open question — confirm with instructor before implementing:** does a room game update ELO when a participant is anonymous? Proposed default: update ELO only if both participants are logged in; otherwise the game runs but ELO is left untouched.

- [ ] **S9 — Multiple concurrent games (registry)**
  `unordered_map<room_id, unique_ptr<GameInstance>>`, each instance with its own mutex (already designed this way since S4 — no retrofit).
  **Done when:** 4 clients, 2 independent concurrent games (or Play+Room simultaneously), no cross-interference.

- [ ] **S10 — Disconnect handling**
  On `on_close`, find the owning `GameInstance`, start a 15s timer (tunable), notify opponent via `opponent_disconnected{seconds_left}`. On timeout: `game_over{winner}`, update ELO (S6), remove instance from registry.
  **Done when:** closing one client mid-game shows a countdown then auto-win on the other side.

- [ ] **S11 — Logging (server + client)**
  Shared `Logger`: timestamped lines to `server.log`/`client.log`. Log message types in/out, errors, disconnect/reconnect, match/room events.
  **Done when:** deliberately breaking something (e.g. disconnecting a client) is clearly traceable in both logs.

- [ ] **S12 — Scaling groundwork** *(deferred — next week, per instructor)*
  Documentation only, no solution code: note that S9's thread-per-game approach won't scale past X concurrent games; a thread-pool/scheduler is the eventual fix.
  **Done when:** the note/issue exists. Do not implement a solution unless explicitly asked.

---

## Dependency table

| # | Stage | Depends on |
|---|---|---|
| S0 | AnimationState decoupling refactor | — (local only) |
| S1 | WebSocket echo | — |
| S2 | Protocol design | S1 |
| S3 | JSON serialization | S1, S2 |
| S4 | Real engine wiring, concurrency, snapshot generation | S0, S1–S3 |
| S5 | Login + SQLite (ELO default 1000) | S4 |
| S6 | ELO calculation | S4, S5 |
| S7 | Matchmaking (±200, ~30s) | S6 |
| S8 | Rooms (spectator enforcement, anonymous hosting) | S4 (S5 only if ELO-for-rooms confirmed) |
| S9 | Multi-game registry | S7, S8 |
| S10 | Disconnect handling | S9 |
| S11 | Logging | S1–S10 |
| S12 | Scaling groundwork (deferred) | S9 |

---

## Session protocol

1. At the start of a new session: read this file fully, state which stage is next (first unchecked box), and confirm before writing any code.
2. Implement only that stage.
3. When its "Done when" criterion is verified: check the box in this file, commit (`git commit -m "S<N> done: <short description>"`), and stop.
4. Never modify a checked-off stage without being explicitly asked to.
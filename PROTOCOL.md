# PROTOCOL.md — Kung Fu Chess wire protocol (S2)

Describes the message set exchanged between `client_main` and `server_main`
(landing at S4). **This is a design document only.** As of S2:

- No JSON. Serialization (`to_json`/`from_json` for every struct here) is S3.
- Not wired to `server_echo.cpp` / `client_echo.cpp` or any other running
  code. S1's echo pair still just echoes raw text.
- Struct definitions live in [`messages.h`](messages.h), namespace `net`.

## Transport & envelope (forward-looking, implemented in S3)

Every message is a single WebSocket text frame containing one JSON object:

```json
{ "type": "<tag>", ...fields }
```

`"type"` is the wire spelling of the corresponding `net::MessageType` enumerator
(snake_case of the enum name, e.g. `MessageType::RoomJoin` → `"room_join"`,
`MessageType::LoginOk` → `"login_ok"`). One connection carries both
directions; which messages are legal at a given moment depends on connection
state (e.g. `ClickMsg` before a game has started is meaningless), which is
server-enforced, not protocol-enforced.

## Message reference

### Client → Server

| Tag | Struct | Fields | Purpose |
|---|---|---|---|
| `login` | `LoginMsg` | `username`, `password` | Authenticate (S5). Reply is `login_ok` or `login_fail`. |
| `play` | `PlayMsg` | *(none)* | Enter ELO-range matchmaking (S7). Server already knows the caller's ELO from their session. Reply is eventually `match_found` or `no_match_found`. |
| `room_join` | `RoomJoinMsg` | `roomName` | Join an existing named room, or create it if no room with that name exists yet (find-or-create) (S8). Does not require login. First two connections to a room name play; the rest spectate. |
| `room_create` | `RoomCreateMsg` | `roomName` | Create a **brand-new** named room (S8). Unlike `room_join`, this does not fall back to joining: if `roomName` is already taken, the server replies `room_create_fail` instead and no join happens. Does not require login. |
| `click` | `ClickMsg` | `row`, `col` | Mirrors `Controller::click(x, y)`. The client resolves its local pixel click to a board cell itself (immune to OS/DPI scaling) and sends the cell, never raw pixels. |
| `jump` | `JumpMsg` | `row`, `col` | Mirrors `Controller::jump(x, y)`. Same row/col convention as `click`. |

No separate select/move message exists — the server's existing selection-state
logic (inside the untouched `Controller::click`) already resolves the
select-then-move ambiguity from two `click` messages; the client does not
duplicate that state machine.

`RoomJoinMsg` and `RoomCreateMsg` carry an identical payload (`roomName`) —
but that's just a shape coincidence, not shared behavior. `room_join` is
find-or-create: it succeeds either way, joining the existing room if one
matches or creating it if none does. `room_create` is create-only: on a name
collision it fails outright (`room_create_fail`) rather than joining the
existing room. Don't infer server-side equivalence from the identical struct
shape.

### Server → Client

| Tag | Struct | Fields | Purpose |
|---|---|---|---|
| `login_ok` | `LoginOkMsg` | `elo` | Login succeeded; carries the persisted ELO (S5). |
| `login_fail` | `LoginFailMsg` | `reason` | Login failed (bad credentials, etc). |
| `match_found` | `MatchFoundMsg` | `opponentUsername`, `assignedColor` | Matchmaking (S7) paired this connection with an opponent. `assignedColor`: first-connect = White, second = Black (S4). |
| `no_match_found` | `NoMatchFoundMsg` | *(none)* | Sent after the ~30s matchmaking window (S7) closes with no in-range opponent. |
| `spectating` | `SpectatingMsg` | `roomName` | Sent to the 3rd+ connection into a room (S8): confirms observer status. `click`/`jump` from this connection are rejected server-side regardless of client UI. |
| `room_create_fail` | `RoomCreateFailMsg` | `reason` | Reply to `room_create` when `roomName` already exists (S8). Shaped like `login_fail`: a single free-text `reason` (e.g. `"room already exists"`). The requested room is left untouched — the caller is not joined to it. |
| `snapshot` | `SnapshotMsg` | see below | Periodic authoritative game-state push driving client rendering. |
| `opponent_disconnected` | `OpponentDisconnectedMsg` | `secondsLeft` | Opponent's connection dropped; countdown to auto-win (S10). |
| `game_over` | `GameOverMsg` | `winner`, `reason` | Game concluded (king captured, or disconnect timeout per S10). |

## `snapshot` in detail

`SnapshotMsg` is the one message type with real internal structure, since it's
what the client's `AnimationState`/render loop consumes every tick. Its shape
is dictated by two settled decisions in `PLAN.md`:

1. **No new `RealTimeArbiter` getters.** The snapshot is built strictly from
   `activeMoveFor(Position)`, `activeRest(pieceId)`, and `activeJump()` — the
   same three calls `image_view.cpp` already uses locally.
2. **Raw, not pre-resolved.** The snapshot carries the actual `PieceMove` /
   `PieceRest` / `PieceJump` structs (their `startMs`/`durationMs`/`untilMs`),
   not a single computed animation frame — the client keeps animating between
   snapshots off its own clock, using the same `resolveAnimationState` /
   `restRemainingFraction` functions `image_view.cpp` uses today (now
   decoupled from `RealTimeArbiter` by S0, so they accept these raw optionals
   directly).

Fields:

- **`pieces`** (`vector<PieceSnapshot>`) — this *is* "board contents": one
  entry per piece currently on the board, each carrying enough of `Piece`
  (`id`, `color`, `kind`, `cell`, `state`, `hasMoved`) to reconstruct it
  client-side, plus that piece's `activeMove` (from `activeMoveFor(cell)`)
  and `activeRest` (from `activeRest(id)`) if any is in progress. There is no
  separate grid representation — any square with no entry is empty.
- **`activeJump`** (`optional<PieceJump>`) — arbiter-wide, not per-piece,
  because `RealTimeArbiter` only ever tracks one in-flight jump at a time
  (`startJump` throws `JUMP_ALREADY_ACTIVE` otherwise). From `activeJump()`.
- **`elapsedMs`** — the server's `GameState.elapsedMs` at snapshot-build time.
  The client anchors the raw `startMs`/`durationMs`/`untilMs` values above
  against this shared clock reading to keep animating smoothly between
  snapshot arrivals.
- **`score`** — `GameState.score` (`Score{white, black}`), unchanged shape.
- **`gameOver`** — `GameState.gameOver` (`GameOverState`), unchanged shape.
- **`moveHistoryDelta`** — **only** the `MoveRecord` entries appended to
  `GameState.moveHistory` since the *last snapshot sent to this connection*
  (per-connection delta, not a global watermark — a newly-joined spectator's
  first snapshot would need its own full-history handling, out of scope for
  S2). Never the full accumulated history.

## Deliberately out of scope for S2

- JSON (de)serialization — S3.
- Anything about *when* a snapshot is sent, at what rate, or under which
  mutex — S4 (per-`GameInstance` mutex, per `PLAN.md`'s concurrency model).
  This doc only defines the payload shape.
- Room/matchmaking *behavior* (ELO range, wait window, spectator rejection
  logic, anonymous-room ELO question) — S7/S8. This doc only defines the
  messages those stages will send.
- Disconnect timer value/logic — S10. This doc only defines the message shape.

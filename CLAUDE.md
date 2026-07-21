# CLAUDE.md — Kung Fu Chess External Server

Project memory for Claude Code. Read this file fully at the start of every session, before touching any file.

## What this project is

A local, real-time, no-turns chess variant ("Kung Fu Chess" — cooldown-based pieces, capture-the-king win condition) is being extended into a networked client/server game (WebSocket, C++, Windows). The existing single-process engine (`GameState`, `RealTimeArbiter`, `GameEngine`, `Controller`, `view::renderBoard`) is being split into a server binary (authoritative game logic) and a client binary (rendering + input only).

## First thing, every session

1. Read `PLAN.md` in full.
2. State out loud which stage is next (the first unchecked `- [ ]`).
3. Confirm with me before writing any code.
4. Work **only** on that one stage. Do not start a later stage "while you're at it," and do not touch a stage already checked off, even if you spot something you'd improve — ask first.

`PLAN.md` also contains the fixed architecture decisions (concurrency/locking model, snapshot design, Controller/input handling). Those are settled — don't re-derive or second-guess them mid-implementation. If something in `PLAN.md` seems to conflict with the code you're looking at, stop and ask rather than resolving it silently in either direction.

## Build & run

- Build system: CMake (`CMakeLists.txt` at repo root, configured into `build/` for MSVC).
- Build command: `cmake --build build --config Debug --target chess_tests` (test suite) / `cmake --build build --config Debug --target chess_game` (local game binary). Future `server_main`/`client_main` targets will follow the same pattern once added.
- Run server: `./build/Debug/server_echo.exe` (S1's proof-of-pipe only — echoes any received message back, no game logic). Real `server_main` arrives at S4.
- Run client: `./build/Debug/client_echo.exe` (S1's proof-of-pipe only — connects, sends `"ping"`, prints the reply). Real `client_main` arrives at S4.
- Package manager: vcpkg, installed at `C:\vcpkg` (cloned from microsoft/vcpkg, bootstrapped via `bootstrap-vcpkg.bat`). `vcpkg integrate install` has been run (applies user-wide MSBuild integration). Installed: `ixwebsocket:x64-windows`, `nlohmann-json:x64-windows`. SQLite via amalgamation (`sqlite3.c`/`sqlite3.h`, not vcpkg — not yet added).
  - Any new CMake target that needs these libraries must configure with `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, then `find_package(ixwebsocket CONFIG REQUIRED)` / `find_package(nlohmann_json CONFIG REQUIRED)` and link `ixwebsocket::ixwebsocket` / `nlohmann_json::nlohmann_json`.
  - **Important environment fact, discovered the hard way during S1:** this machine has *two* separate Visual Studio installations — VS 2022 Community (toolset `14.44.35207`, at `C:\Program Files\Microsoft Visual Studio\2022\Community`, what the project's `build/` directory is configured for) and a separate, newer "Visual Studio 18" BuildTools install (toolset `14.51.36231`). Left to its own detection, vcpkg (when run from a shell without `vcvars64.bat` loaded, e.g. plain Bash) picks the newer one, which produces STL-ABI-incompatible static libs when later linked against the older toolset (symptom: `unresolved external symbol __std_find_first_not_of_trivial_pos_1` or similar STL-internal symbols at link time — not a missing-library problem, a toolset mismatch). Fixed by pinning `VCPKG_VISUAL_STUDIO_PATH` (user env var, set via `setx`, persists machine-wide) to the VS 2022 Community path, then reinstalling any already-built packages so they're rebuilt under the matching toolset. If this class of unresolved-STL-symbol error ever reappears, check this first before assuming a missing dependency.
  - `build/` was regenerated with `-G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake` once vcpkg packages were actually needed (S1).
- Test suite: `./build/Debug/chess_tests.exe` (doctest). Must show 0 failed before any stage is marked done. Baseline as of S0 start: 139 test cases / 438 assertions, all passing. Still 139/139 after S1's toolchain changes.

## Hard rules — don't do these without asking first

- **Don't add a new dependency** (library, vcpkg package, header) that isn't already listed above, even if it would make a stage easier. Ask first.
- **Don't touch `Controller.cpp`'s signatures.** `Controller::click`/`Controller::jump` keep taking raw pixel `(x, y)` — this is a deliberate, settled decision (see `PLAN.md`). New networking logic is glue code that lives outside this file.
- **Don't add getters to `RealTimeArbiter`.** Snapshot building must use only the existing public API (`activeMoveFor`, `activeRest`, `activeJump`). This is settled — see `PLAN.md`.
- **Don't use a single global mutex.** Locking is per-`GameInstance`, plus separate independent locks for `MatchmakingPool` and `RoomManager`. Never conflate them.
- **Don't hardcode pixel/cell math.** Use the shared `config::CELL_SIZE` / `config::HISTORY_PANEL_WIDTH` constants so client and server agree by construction.
- **Don't refactor or "clean up" code outside the current stage's scope**, even if it looks improvable. Flag it to me instead; I'll decide if it becomes its own stage.
- **No debug `printf`/`cout` left in committed code.** This codebase is graded/tested by an automated grader (VPL-style) elsewhere in the course — treat leftover debug output as a bug, not a convenience. Remove it before considering a stage done.
- **Don't guess on ambiguity.** If the spec, `PLAN.md`, or existing code leaves something genuinely unclear, ask a short, specific question instead of picking a default and moving on. Wrong assumptions compound across stages.

## Git — commit/push are mine, everything else is fine

I own `git add`, `git commit`, and `git push` exclusively — never run these three, not even a "harmless" checkbox-only commit, not even if I previously approved a commit message format. When a stage is done, just tell me what changed and suggest a commit message in your reply; do not stage, commit, or push anything yourself.

Other git operations (e.g. `git mv`, `git rm`, `git checkout`, `git reset --hard` on a file, etc.) are fine for you to run directly when a stage genuinely calls for them — just tell me what you ran and why in your reply, so I have full visibility.

## Workflow for finishing a stage

1. Implement the current stage only.
2. Build clean, run the stage's "Done when" check from `PLAN.md` yourself, and show me the actual output/result — don't just claim it works.
3. Once I confirm it's good: check the box in `PLAN.md` (a file edit, not a git operation — that part's fine). Then tell me what changed and propose a commit message, one stage per commit — but leave staging/committing/pushing to me, per the Git section above.
4. Stop. Don't continue to the next stage automatically, even if the path forward seems obvious.

## Code style

- Match the existing naming/formatting conventions already present in the file you're editing rather than introducing a new style — check a neighboring file if unsure.
- Prefer small, reviewable diffs over large rewrites, even inside a single stage.
- Comment *why*, not *what*, and only where the reasoning genuinely isn't obvious from the code (e.g. why a lock is held here, not what a mutex is).

## If something goes wrong mid-stage

Don't silently patch around a failure by touching a previous stage's code. Stop, explain what broke and why you think a previous stage is implicated, and ask before making any change outside the current stage's files.
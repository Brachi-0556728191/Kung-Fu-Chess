#include "doctest.h"

#include "../input/Controller.hpp"
#include "../engine/GameEngine.hpp"
#include "../texttests/ScriptRunner.hpp"
#include "../realtime/RealTimeArbiter.hpp"
#include "../model/Board.hpp"
#include "../model/GameState.hpp"
#include "../model/Piece.hpp"
#include "../io/BoardParser.hpp"
#include "../io/BoardPrinter.hpp"
#include "../rules/config.hpp"

#include <sstream>
#include <iostream>

namespace {
    GameState makeState(const std::vector<std::string>& boardLines) {
        GameState st;
        st.board = parseBoard(boardLines);
        return st;
    }

    // All click/jump pixel tests below target board-relative pixels; the
    // actual window has a history panel to the left of the board (see
    // view::renderBoard / BoardMapper::pixelToCell), so every real x
    // coordinate must be shifted by that panel's width to land on the
    // intended cell.
    int boardPx(int x) { return config::HISTORY_PANEL_WIDTH + x; }

    std::string tokenAt(const Board& b, Position pos) {
        auto piece = b.pieceAt(pos);
        return piece ? tokenFromPiece(*piece) : EMPTY_TOKEN;
    }

    // advanceTime now returns one ArrivalEvent per motion that settled this
    // call, since several can be in flight at once. Most existing tests only
    // ever register a single motion, so this asserts exactly one came back
    // and unwraps it - keeping those tests' bodies unchanged otherwise.
    ArrivalEvent onlyEvent(const std::vector<ArrivalEvent>& events) {
        REQUIRE(events.size() == 1);
        return events[0];
    }
}

TEST_CASE("advanceTime keeps a move active before its arrival time") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 1000; m.piece = "wR";
    st.arbiter.startMotion(m);
    st.elapsedMs = 500;

    st.arbiter.advanceTime(st.elapsedMs, st.board);

    REQUIRE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == EMPTY_TOKEN);
}

TEST_CASE("advanceTime lands a move onto an empty target once due") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 1000; m.piece = "wR";
    st.arbiter.startMotion(m);
    st.elapsedMs = 1000;

    st.arbiter.advanceTime(st.elapsedMs, st.board);

    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("advanceTime: a move must not arrive at 999ms but must arrive at exactly 1000ms") {
    GameState st = makeState({"wR . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 1};
    m.startMs = 0; m.durationMs = 1000; m.piece = "wR";
    st.arbiter.startMotion(m);

    st.arbiter.advanceTime(999, st.board);
    CHECK(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 1}) == EMPTY_TOKEN);

    st.arbiter.advanceTime(1000, st.board);
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 1}) == "wR");
}

TEST_CASE("advanceTime accumulates partial time correctly across multiple calls") {
    GameState st = makeState({"wR . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 1};
    m.startMs = 0; m.durationMs = 1000; m.piece = "wR";
    st.arbiter.startMotion(m);

    long elapsed = 0;

    elapsed += 300;
    st.arbiter.advanceTime(elapsed, st.board);   // 300ms total
    CHECK(st.arbiter.hasActiveMotion());

    elapsed += 400;
    st.arbiter.advanceTime(elapsed, st.board);   // 700ms total
    CHECK(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 1}) == EMPTY_TOKEN);

    elapsed += 299;
    st.arbiter.advanceTime(elapsed, st.board);   // 999ms total - still not due
    CHECK(st.arbiter.hasActiveMotion());

    elapsed += 1;
    st.arbiter.advanceTime(elapsed, st.board);   // 1000ms total - arrives now
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 1}) == "wR");
}

TEST_CASE("advanceTime settles multiple independent concurrent motions in the same call") {
    // Kung Fu Chess: two unrelated pieces can be mid-flight at once, and each
    // must land correctly without the other interfering.
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", "wN . . ."});

    PieceMove rookMove; rookMove.from = {0, 0}; rookMove.to = {0, 3};
    rookMove.startMs = 0; rookMove.durationMs = 500; rookMove.piece = "wR";
    st.arbiter.startMotion(rookMove);

    PieceMove knightMove; knightMove.from = {3, 0}; knightMove.to = {2, 2};
    knightMove.startMs = 0; knightMove.durationMs = 300; knightMove.piece = "wN";
    st.arbiter.startMotion(knightMove);

    REQUIRE(st.arbiter.isPieceCurrentlyMoving({0, 0}));
    REQUIRE(st.arbiter.isPieceCurrentlyMoving({3, 0}));

    // The knight (300ms) is due before the rook (500ms), but both are due by
    // 500ms and settle within this single call, in arrival order.
    std::vector<ArrivalEvent> events = st.arbiter.advanceTime(500, st.board);

    CHECK(events.size() == 2);
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
    CHECK(tokenAt(st.board, {2, 2}) == "wN");
}

TEST_CASE("advanceTime captures an enemy occupying the target") {
    GameState st = makeState({"wR . . bP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);
    st.elapsedMs = 500;

    st.arbiter.advanceTime(st.elapsedMs, st.board);

    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("advanceTime leaves a piece at its source when the target turned friendly") {
    // With the mid-flight-disappearance bug fixed, the source piece is never
    // removed in the first place, so there is nothing to "restore" - it
    // simply never left. The board fixture reflects that reality (the wR is
    // actually still sitting at its source).
    GameState st = makeState({"wR . . wP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);
    st.elapsedMs = 500;

    st.arbiter.advanceTime(st.elapsedMs, st.board);

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    CHECK(tokenAt(st.board, {0, 3}) == "wP");
}

TEST_CASE("advanceTime loses a piece when the target turned friendly and the source is occupied") {
    GameState st = makeState({"wN . . wP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);
    st.elapsedMs = 500;

    st.arbiter.advanceTime(st.elapsedMs, st.board);

    CHECK(tokenAt(st.board, {0, 0}) == "wN");
    CHECK(tokenAt(st.board, {0, 3}) == "wP");
}

TEST_CASE("advanceTime's ArrivalEvent reports a captured piece with Captured state") {
    GameState st = makeState({"wR . . bP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(500, st.board));

    CHECK(event.pieceArrived);
    REQUIRE(event.capturedPiece.has_value());
    CHECK(event.capturedPiece->kind == Kind::Pawn);
    CHECK(event.capturedPiece->color == Color::Black);
    CHECK(event.capturedPiece->state == PieceState::Captured);
}

TEST_CASE("advanceTime's ArrivalEvent has no captured piece when landing on an empty cell") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(500, st.board));

    CHECK(event.pieceArrived);
    CHECK_FALSE(event.capturedPiece.has_value());
}

TEST_CASE("advanceTime's ArrivalEvent reports pieceArrived false when the target turned friendly") {
    GameState st = makeState({"wR . . wP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(500, st.board));

    CHECK_FALSE(event.pieceArrived);
    CHECK_FALSE(event.capturedPiece.has_value());
}

TEST_CASE("advanceTime promotes a white pawn to a queen when it reaches the last row") {
    GameState st = makeState({". . .", ". . .", "wP . ."});
    PieceMove m; m.from = {2, 0}; m.to = {0, 0};
    m.startMs = 0; m.durationMs = 500; m.piece = "wP";
    st.arbiter.startMotion(m);

    st.arbiter.advanceTime(500, st.board);

    CHECK(tokenAt(st.board, {0, 0}) == "wQ");
}

TEST_CASE("advanceTime promotes a black pawn reaching the last row for its color") {
    GameState st = makeState({"bP . .", ". . .", ". . ."});
    PieceMove m; m.from = {0, 0}; m.to = {2, 0};
    m.startMs = 0; m.durationMs = 500; m.piece = "bP";
    st.arbiter.startMotion(m);

    st.arbiter.advanceTime(500, st.board);

    CHECK(tokenAt(st.board, {2, 0}) == "bQ");
}

TEST_CASE("advanceTime leaves a pawn as a pawn when it doesn't reach the last row") {
    GameState st = makeState({". . .", ". . .", "wP . ."});
    PieceMove m; m.from = {2, 0}; m.to = {1, 0};
    m.startMs = 0; m.durationMs = 500; m.piece = "wP";
    st.arbiter.startMotion(m);

    st.arbiter.advanceTime(500, st.board);

    CHECK(tokenAt(st.board, {1, 0}) == "wP");
}

TEST_CASE("advanceTime does not promote a non-pawn piece that reaches what would be a pawn's last row") {
    GameState st = makeState({". . .", ". . .", "wR . ."});
    PieceMove m; m.from = {2, 0}; m.to = {0, 0};
    m.startMs = 0; m.durationMs = 500; m.piece = "wR";
    st.arbiter.startMotion(m);

    st.arbiter.advanceTime(500, st.board);

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
}

TEST_CASE("startJump activates a jump for an idle piece") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    CHECK_FALSE(st.arbiter.hasActiveJump());

    st.arbiter.startJump({0, 0}, 0);

    CHECK(st.arbiter.hasActiveJump());
    CHECK(st.arbiter.isPieceCurrentlyJumping({0, 0}));
}

TEST_CASE("startJump throws if a jump is already active") {
    GameState st = makeState({"wR . wN", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);

    CHECK_THROWS_AS(st.arbiter.startJump({0, 2}, 0), RealTimeArbiterError);
}

TEST_CASE("startJump throws for a piece that is currently the active move's source") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 2}; m.startMs = 0; m.durationMs = 1000; m.piece = "wR";
    st.arbiter.startMotion(m);

    CHECK_THROWS_AS(st.arbiter.startJump({0, 0}, 0), RealTimeArbiterError);
}

TEST_CASE("advanceTime leaves an active jump untouched before its 1000ms window elapses") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);

    st.arbiter.advanceTime(999, st.board);

    CHECK(st.arbiter.hasActiveJump());
    CHECK(tokenAt(st.board, {0, 0}) == "wR");
}

TEST_CASE("advanceTime lands a jump with zero board change once its window elapses") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);

    st.arbiter.advanceTime(1000, st.board);

    CHECK_FALSE(st.arbiter.hasActiveJump());
    CHECK(tokenAt(st.board, {0, 0}) == "wR");
}

TEST_CASE("advanceTime intercepts an enemy move arriving at an active jump's cell") {
    GameState st = makeState({"wR . bN", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);   // wR jumps in place

    PieceMove m; m.from = {0, 2}; m.to = {0, 0}; m.startMs = 0; m.durationMs = 999; m.piece = "bN";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(999, st.board));

    CHECK_FALSE(st.arbiter.hasActiveJump());
    CHECK(tokenAt(st.board, {0, 0}) == "wR");          // jumper unchanged, still present
    CHECK(tokenAt(st.board, {0, 2}) == EMPTY_TOKEN);   // arriving piece never lands anywhere
    REQUIRE(event.capturedPiece.has_value());
    CHECK(event.capturedPiece->kind == Kind::Knight);
    CHECK(event.capturedPiece->color == Color::Black);
    CHECK(event.capturedPiece->state == PieceState::Captured);
}

TEST_CASE("advanceTime treats a friendly arrival at a jump's cell as an ordinary friendly-block, not interception") {
    GameState st = makeState({"wR . wN", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);   // wR jumps in place

    PieceMove m; m.from = {0, 2}; m.to = {0, 0}; m.startMs = 0; m.durationMs = 999; m.piece = "wN";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(999, st.board));

    CHECK(st.arbiter.hasActiveJump());                 // jump unaffected, still active
    CHECK(tokenAt(st.board, {0, 0}) == "wR");           // jumper still there
    CHECK(tokenAt(st.board, {0, 2}) == "wN");           // friendly move lost, arriving piece stays put
    CHECK_FALSE(event.pieceArrived);
    CHECK_FALSE(event.capturedPiece.has_value());
}

TEST_CASE("advanceTime resolves an ordinary capture when a move arrives after the jump already expired") {
    GameState st = makeState({"wR . bN", ". . .", ". . ."});
    st.arbiter.startJump({0, 0}, 0);
    st.arbiter.advanceTime(1000, st.board);            // jump expires independently first
    REQUIRE_FALSE(st.arbiter.hasActiveJump());

    PieceMove m; m.from = {0, 2}; m.to = {0, 0}; m.startMs = 1000; m.durationMs = 500; m.piece = "bN";
    st.arbiter.startMotion(m);

    ArrivalEvent event = onlyEvent(st.arbiter.advanceTime(1500, st.board));

    CHECK(tokenAt(st.board, {0, 0}) == "bN");          // ordinary capture-by-landing, not interception
    REQUIRE(event.capturedPiece.has_value());
    CHECK(event.capturedPiece->kind == Kind::Rook);
}

TEST_CASE("sendMove keeps the piece at its source and queues an active motion for a legal move") {
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    sendMove(st, 0, 3);

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    CHECK(st.arbiter.hasActiveMotion());
    CHECK_FALSE(st.selection.active);

    // Confirm the queued motion is indeed the rook heading to (0,3): advance
    // past its travel time (1.0 cells/sec, 3 cells -> 3000ms) and check it
    // actually arrives there.
    st.arbiter.advanceTime(3000, st.board);
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("sendMove ignores an illegal move and leaves the board untouched") {
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    sendMove(st, 1, 1); // diagonal - illegal for a rook

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("sendMove computes duration from piece speed and travel distance") {
    GameState st = makeState({"wQ . . .", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    sendMove(st, 0, 3); // queen: 1.0 cells/sec, 3 cells travelled -> 3000ms

    REQUIRE(st.arbiter.hasActiveMotion());

    // Not yet arrived just before the computed duration...
    st.arbiter.advanceTime(2999, st.board);
    CHECK(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == EMPTY_TOKEN);

    // ...but arrived exactly at 3000ms, confirming the computed duration.
    st.arbiter.advanceTime(3000, st.board);
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wQ");
}

TEST_CASE("sendMove accepts a second move for a different, unrelated piece while the first is still in flight") {
    // Kung Fu Chess has no turns: pieces move independently and
    // simultaneously, so one piece's in-flight motion must never block an
    // unrelated piece's otherwise-legal move.
    GameState st = makeState({"wR . . .", ". . . wN", ". . . .", ". . . ."});

    // A: select the rook at (0,0) and send it toward (0,3).
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3);
    REQUIRE(st.arbiter.isPieceCurrentlyMoving({0, 0}));

    // B: select and move the knight at (1,3) to (3,2) - a legal knight move
    // on an empty square - while A is still mid-flight.
    st.selection = {true, {1, 3}, 0};
    sendMove(st, 3, 2);

    // B's motion is accepted independently of A's: it's now in flight too,
    // not yet arrived, and its selection cleared like any completed request.
    CHECK(st.arbiter.isPieceCurrentlyMoving({1, 3}));
    CHECK_FALSE(st.selection.active);
    CHECK(tokenAt(st.board, {1, 3}) == "wN");

    // Both arrive independently, without interfering with each other:
    // the knight (2 cells -> 2000ms) lands before the rook (3 cells ->
    // 3000ms), and neither disturbs the other's destination.
    st.arbiter.advanceTime(3000, st.board);
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
    CHECK(tokenAt(st.board, {3, 2}) == "wN");
}

TEST_CASE("sendMove rejects a second move for the SAME piece while its own motion is still in flight") {
    // The per-piece guard that replaced the old global one must still catch
    // the case it was actually protecting against: a piece can't be sent on
    // a new move while it's already mid-flight from an earlier one.
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});

    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3); // 3 cells -> 3000ms
    REQUIRE(st.arbiter.isPieceCurrentlyMoving({0, 0}));

    // Re-select the same rook mid-flight and attempt another move - rejected.
    st.selection = {true, {0, 0}, st.elapsedMs};
    sendMove(st, 1, 0);

    CHECK(st.arbiter.isPieceCurrentlyMoving({0, 0}));
    CHECK_FALSE(st.selection.active);
    CHECK(tokenAt(st.board, {1, 0}) == EMPTY_TOKEN); // the rejected move never happened

    st.arbiter.advanceTime(3000, st.board);
    CHECK(tokenAt(st.board, {0, 3}) == "wR"); // original motion landed untouched
}

TEST_CASE("sendMove rejects a new move right after the previous motion completes: long_rest cooldown") {
    // A completed move now starts a long_rest cooldown (config::statsFor's
    // longRestMs) on the piece that just arrived - it can't be sent on
    // another move until that cooldown expires.
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});

    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3); // rook: 3 cells at 1.0 cells/sec -> 3000ms
    REQUIRE(st.arbiter.hasActiveMotion());

    handleWait(st, 3000); // arrives exactly on time; long_rest starts now
    REQUIRE_FALSE(st.arbiter.hasActiveMotion());
    REQUIRE(tokenAt(st.board, {0, 3}) == "wR");

    // Immediately (zero additional wait) request a new move - it must be
    // rejected: the rook is still resting.
    st.selection = {true, {0, 3}, st.elapsedMs};
    sendMove(st, 0, 0);

    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
    CHECK_FALSE(st.selection.active);

    // Wait past longRestMs (1500ms) - the cooldown is over, so the same move
    // now succeeds.
    handleWait(st, 1500);
    st.selection = {true, {0, 3}, st.elapsedMs};
    sendMove(st, 0, 0); // rook travels back, same 3-cell distance -> 3000ms

    CHECK(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wR"); // still at source until this motion arrives

    handleWait(st, 3000);
    CHECK(tokenAt(st.board, {0, 0}) == "wR");
}

TEST_CASE("handleWait sets gameOver true when a king is captured on arrival") {
    GameState st = makeState({"wR . . bK", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    sendMove(st, 0, 3); // rook captures the black king
    REQUIRE(st.arbiter.hasActiveMotion());
    CHECK_FALSE(st.gameOver);

    handleWait(st, 3000); // 3 cells at 1.0 cells/sec -> 3000ms

    CHECK(st.gameOver);
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("handleWait records the capturing side as the winner when a king is captured") {
    GameState st = makeState({"wR . . bK", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3); // white rook captures the black king

    handleWait(st, 3000);

    REQUIRE(st.gameOver);
    CHECK(st.gameOver.winner == Color::White);
    CHECK(st.gameOver.reason == GameOverReason::KingCaptured);
}

TEST_CASE("handleWait records black as the winner when black captures the white king") {
    GameState st = makeState({"bR . . wK", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3); // black rook captures the white king

    handleWait(st, 3000);

    REQUIRE(st.gameOver);
    CHECK(st.gameOver.winner == Color::Black);
}

TEST_CASE("handleWait records the jumper's side as the winner when a jump interception destroys the enemy king") {
    GameState st = makeState({"wR bK .", ". . .", ". . ."});
    startJump(st, {0, 0});    // wR jumps in place

    st.selection = {true, {0, 1}, 0};
    sendMove(st, 0, 0);       // bK attempts to move onto wR's cell

    handleWait(st, 1000);     // the king's move and the jump's window elapse at the same tick

    REQUIRE(st.gameOver);
    CHECK(st.gameOver.winner == Color::White);   // the jumper's side, not the destroyed king's
}

TEST_CASE("a selection active when the game ends is cleared, so no stale highlight lingers") {
    GameState st = makeState({"wR . . bK", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3); // rook captures the black king
    REQUIRE(st.selection.active == false);   // sendMove already clears it on send

    // Re-open a selection mid-flight, before the capturing move has landed,
    // to prove handleWait itself clears it once the game actually ends -
    // not merely as a side effect of sendMove having run earlier.
    st.selection = {true, {1, 1}, st.elapsedMs};

    handleWait(st, 3000);

    REQUIRE(st.gameOver);
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("sendMove after game over leaves the board completely unchanged") {
    GameState st = makeState({"wR . . bK", ". . . wN", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3);
    handleWait(st, 3000);
    REQUIRE(st.gameOver);

    const std::string knightBefore = tokenAt(st.board, {1, 3});
    const std::string rookBefore   = tokenAt(st.board, {0, 3});

    st.selection = {true, {1, 3}, st.elapsedMs};
    sendMove(st, 3, 2); // otherwise-legal knight move, attempted after game over

    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {1, 3}) == knightBefore);
    CHECK(tokenAt(st.board, {0, 3}) == rookBefore);
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("capturing a non-king piece does not end the game") {
    GameState st = makeState({"wR . . bP", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    sendMove(st, 0, 3); // rook captures the black pawn
    handleWait(st, 3000);

    CHECK_FALSE(st.gameOver);
    CHECK(tokenAt(st.board, {0, 3}) == "wR"); // capture went through normally
}

TEST_CASE("startJump succeeds for an idle, non-moving, non-jumping piece") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});

    startJump(st, {0, 0});

    CHECK(st.arbiter.hasActiveJump());
    CHECK(st.arbiter.isPieceCurrentlyJumping({0, 0}));
}

TEST_CASE("startJump is a silent no-op for a piece that is currently the active move's source") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 2);
    REQUIRE(st.arbiter.hasActiveMotion());

    startJump(st, {0, 0});

    CHECK_FALSE(st.arbiter.hasActiveJump());
}

TEST_CASE("startJump is a silent no-op while a jump is already active anywhere on the board") {
    GameState st = makeState({"wR . wN", ". . .", ". . ."});
    startJump(st, {0, 0});
    REQUIRE(st.arbiter.hasActiveJump());

    startJump(st, {0, 2});

    CHECK(st.arbiter.isPieceCurrentlyJumping({0, 0}));
    CHECK_FALSE(st.arbiter.isPieceCurrentlyJumping({0, 2}));
}

TEST_CASE("sendMove is rejected for a piece that is currently jumping") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});
    startJump(st, {0, 0});
    REQUIRE(st.arbiter.hasActiveJump());

    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 2);

    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("sendMove and startJump both no-op once the game is over") {
    GameState st = makeState({"wR . . bK", ". . . wN", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3);      // rook captures the black king
    handleWait(st, 3000);
    REQUIRE(st.gameOver);

    st.selection = {true, {1, 3}, st.elapsedMs};
    sendMove(st, 3, 2);      // otherwise-legal knight move
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {1, 3}) == "wN");

    startJump(st, {1, 3});
    CHECK_FALSE(st.arbiter.hasActiveJump());
}

TEST_CASE("a king destroyed via jump interception sets gameOver true") {
    GameState st = makeState({"wR bK .", ". . .", ". . ."});

    startJump(st, {0, 0});    // wR jumps in place

    st.selection = {true, {0, 1}, 0};
    sendMove(st, 0, 0);       // bK attempts to move onto wR's cell

    REQUIRE(st.arbiter.hasActiveMotion());
    REQUIRE(st.arbiter.hasActiveJump());

    handleWait(st, 1000);     // the king's move and the jump's window elapse at the same tick

    CHECK(st.gameOver);
    CHECK(tokenAt(st.board, {0, 0}) == "wR");    // jumper survives, unmoved
    CHECK(tokenAt(st.board, {0, 1}) == EMPTY_TOKEN);
    CHECK_FALSE(st.arbiter.hasActiveJump());     // jump concluded via interception
}

TEST_CASE("handleClick opens a fresh selection when clicking an idle piece") {
    GameState st = makeState({"wK . . .", ". . . .", ". . . .", ". . . ."});

    Controller::click(st, boardPx(5), 5); // inside cell (0,0)

    CHECK(st.selection.active);
    CHECK(st.selection.cell.row == 0);
    CHECK(st.selection.cell.col == 0);
}

TEST_CASE("handleClick reselects when clicking another piece of the same color") {
    GameState st = makeState({"wK . wQ .", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    Controller::click(st, boardPx(205), 5); // cell (0,2), also white

    CHECK(st.selection.active);
    CHECK(st.selection.cell.col == 2);
}

TEST_CASE("handleClick completes a pending selection when clicking an empty cell") {
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    Controller::click(st, boardPx(305), 5); // empty cell (0,3)

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    REQUIRE(st.arbiter.hasActiveMotion());

    st.arbiter.advanceTime(3000, st.board);
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("handleClick completes a pending selection as a capture on an enemy cell") {
    GameState st = makeState({"wR . . bP", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};

    Controller::click(st, boardPx(305), 5); // the black pawn at (0,3)

    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    REQUIRE(st.arbiter.hasActiveMotion());
    CHECK_FALSE(st.selection.active);

    st.arbiter.advanceTime(3000, st.board);
    CHECK(tokenAt(st.board, {0, 3}) == "wR"); // captured the black pawn
}

TEST_CASE("handleClick with no pending selection opens a selection regardless of piece color") {
    GameState st = makeState({"wR . . bP", ". . . .", ". . . .", ". . . ."});

    Controller::click(st, boardPx(305), 5); // black's pawn, nobody is pending

    CHECK(tokenAt(st.board, {0, 3}) == "bP");
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(st.selection.active);
    CHECK(st.selection.cell.col == 3);
}

TEST_CASE("handleClick ignores clicks outside the board") {
    GameState st = makeState({"wK ."});
    Controller::click(st, -5, -5);
    Controller::click(st, 10000, 10000);

    CHECK_FALSE(st.selection.active);
}

TEST_CASE("handleClick cancels an active selection on an outside-board click") {
    GameState st = makeState({"wR . . bN", ". . . .", ". . . .", ". . . ."});

    Controller::click(st, boardPx(5), 5); // select the rook at (0,0)
    REQUIRE(st.selection.active);

    Controller::click(st, -5, -5); // outside the board -> must cancel the selection

    CHECK_FALSE(st.selection.active);

    // The next in-bounds click must open a fresh selection on the enemy
    // knight, not be misread as completing a move from the stale selection.
    Controller::click(st, boardPx(305), 5); // click on the black knight at (0,3)

    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 0}) == "wR");   // rook never moved
    CHECK(st.selection.active);
    CHECK(st.selection.cell.col == 3);          // freshly selected the knight, not a completed move
}

TEST_CASE("handleClick on an empty cell with no pending selection is a no-op") {
    GameState st = makeState({". . .", ". . .", ". . ."});
    Controller::click(st, boardPx(5), 5);
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("handleClick refuses to open a new selection once the game is over") {
    GameState st = makeState({"wR . . bK", ". . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3);
    handleWait(st, 3000);
    REQUIRE(st.gameOver);
    REQUIRE(tokenAt(st.board, {0, 3}) == "wR");

    Controller::click(st, boardPx(305), 5); // click the white rook now sitting at (0,3)
    CHECK_FALSE(st.selection.active);
}

TEST_CASE("handleClick cannot reselect a different piece once the game is over") {
    // Guards against a subtler gap than the "open a fresh selection" case:
    // if a selection were somehow still active post-game-over, the
    // "reselect a same-side piece" branch must also refuse to run.
    GameState st = makeState({"wR . . bK", "wN . . .", ". . . .", ". . . ."});
    st.selection = {true, {0, 0}, 0};
    sendMove(st, 0, 3);
    handleWait(st, 3000);
    REQUIRE(st.gameOver);

    st.selection = {true, {0, 3}, st.elapsedMs};   // force a selection back open, bypassing Controller
    Controller::click(st, boardPx(5), 105); // click the white knight at (1,0) - same side

    CHECK(st.selection.active);        // untouched: click() returned before touching selection at all
    CHECK(st.selection.cell.row == 0); // still the rook, not reselected onto the knight
    CHECK(st.selection.cell.col == 3);
}

TEST_CASE("handleWait advances the clock and resolves due moves") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.elapsedMs == 150);
    CHECK_FALSE(st.arbiter.hasActiveMotion());
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("handleWait records a MoveRecord in GameState::moveHistory when a move lands") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    REQUIRE(st.moveHistory.size() == 1);
    const MoveRecord& rec = st.moveHistory[0];
    CHECK(rec.color == Color::White);
    CHECK(rec.kind == Kind::Rook);
    CHECK(rec.from == Position{0, 0});
    CHECK(rec.to == Position{0, 3});
    CHECK_FALSE(rec.captured);
}

TEST_CASE("handleWait's recorded MoveRecord has captured=true when the arrival is a capture") {
    GameState st = makeState({"wR . . bP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    REQUIRE(st.moveHistory.size() == 1);
    CHECK(st.moveHistory[0].captured);
}

TEST_CASE("handleWait does not record a MoveRecord for a friendly-blocked arrival") {
    GameState st = makeState({"wR . . wP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.moveHistory.empty());
}

TEST_CASE("handleWait does not record a MoveRecord for a piece destroyed mid-flight by jump interception") {
    GameState st = makeState({"wR . bN"});
    st.arbiter.startJump({0, 0}, 0);   // wR jumps in place

    PieceMove m; m.from = {0, 2}; m.to = {0, 0}; m.startMs = 0; m.durationMs = 999; m.piece = "bN";
    st.arbiter.startMotion(m);

    handleWait(st, 999);

    // The black knight never completed a move - it was destroyed in transit -
    // so it must not appear in the history, even though an ArrivalEvent with
    // a capturedPiece was produced.
    CHECK(st.moveHistory.empty());
}

TEST_CASE("handleWait appends multiple independent MoveRecords in arrival order") {
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", "wN . . ."});

    PieceMove rookMove; rookMove.from = {0, 0}; rookMove.to = {0, 3};
    rookMove.startMs = 0; rookMove.durationMs = 500; rookMove.piece = "wR";
    st.arbiter.startMotion(rookMove);

    PieceMove knightMove; knightMove.from = {3, 0}; knightMove.to = {2, 2};
    knightMove.startMs = 0; knightMove.durationMs = 300; knightMove.piece = "wN";
    st.arbiter.startMotion(knightMove);

    handleWait(st, 500);

    REQUIRE(st.moveHistory.size() == 2);
    CHECK(st.moveHistory[0].kind == Kind::Knight);   // due at 300ms, settles first
    CHECK(st.moveHistory[1].kind == Kind::Rook);      // due at 500ms, settles second
}

TEST_CASE("a fresh GameState starts with zero score for both sides") {
    GameState st = makeState({"wR . . ."});
    CHECK(st.score.white == 0);
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait credits the capturing side's score with the captured piece's point value") {
    GameState st = makeState({"wR . . bN"});   // Knight = 3 points
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.score.white == 3);
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait does not change score when an arrival is not a capture") {
    GameState st = makeState({"wR . . ."});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.score.white == 0);
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait does not change score for a friendly-blocked arrival") {
    GameState st = makeState({"wR . . wP"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.score.white == 0);
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait credits black's score when black captures a white piece") {
    GameState st = makeState({"bQ . . wR"});   // Rook = 5 points
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "bQ";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.score.black == 5);   // captured piece's (Rook's) value counts, not the capturing Queen's
    CHECK(st.score.white == 0);
}

TEST_CASE("handleWait credits the jumper's side when a jump interception destroys the arriving piece") {
    // The jumper (white) survives; the arriving black knight is destroyed
    // mid-flight - the interception is a capture credited to white, exactly
    // like an ordinary capture, via the same event.capturedPiece field.
    GameState st = makeState({"wR . bN"});
    st.arbiter.startJump({0, 0}, 0);

    PieceMove m; m.from = {0, 2}; m.to = {0, 0}; m.startMs = 0; m.durationMs = 999; m.piece = "bN";
    st.arbiter.startMotion(m);

    handleWait(st, 999);

    CHECK(st.score.white == 3);   // Knight = 3 points
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait accumulates score across multiple independent captures") {
    GameState st = makeState({"wR . . bP", ". bB . .", ". . . .", "wN . . ."});

    PieceMove rookMove; rookMove.from = {0, 0}; rookMove.to = {0, 3};   // captures bP (1 point)
    rookMove.startMs = 0; rookMove.durationMs = 500; rookMove.piece = "wR";
    st.arbiter.startMotion(rookMove);

    PieceMove knightMove; knightMove.from = {3, 0}; knightMove.to = {1, 1};   // captures bB (3 points)
    knightMove.startMs = 0; knightMove.durationMs = 300; knightMove.piece = "wN";
    st.arbiter.startMotion(knightMove);

    handleWait(st, 500);

    CHECK(st.score.white == 4);   // 1 + 3, accumulated across two concurrent captures
    CHECK(st.score.black == 0);
}

TEST_CASE("handleWait awards zero points for a captured King even though it ends the game") {
    GameState st = makeState({"wR . . bK"});
    PieceMove m; m.from = {0, 0}; m.to = {0, 3};
    m.startMs = 0; m.durationMs = 100; m.piece = "wR";
    st.arbiter.startMotion(m);

    handleWait(st, 150);

    CHECK(st.gameOver);
    CHECK(st.score.white == 0);   // King = 0 points by design
    CHECK(st.score.black == 0);
}

TEST_CASE("runCommands executes click, wait and print in sequence") {
    GameState st = makeState({"wR . . .", ". . . .", ". . . .", ". . . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "click " + std::to_string(boardPx(5)) + " 5",
        "click " + std::to_string(boardPx(305)) + " 5",
        "wait 3000", // rook: 1.0 cells/sec, 3 cells travelled -> 3000ms to arrive
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {0, 3}) == "wR");
}

TEST_CASE("runCommands demonstrates a pawn's two-square opening move with a clear path") {
    // 4 rows so the landing square (row 1) is NOT the last row - this test is
    // about the two-square move itself, not promotion, so it must land the
    // pawn somewhere it stays a pawn.
    GameState st = makeState({". . .", ". . .", ". . .", "wP . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "click " + std::to_string(boardPx(5)) + " 305",   // select the pawn at (3,0)
        "click " + std::to_string(boardPx(5)) + " 105",   // move it two squares to (1,0)
        "wait 2000",     // pawn: 1.0 cells/sec, 2 cells travelled -> 2000ms
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {1, 0}) == "wP");
}

TEST_CASE("runCommands demonstrates a pawn promoting to a queen upon reaching the last row") {
    GameState st = makeState({". . .", "wP . .", ". . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "click " + std::to_string(boardPx(5)) + " 105",   // select the pawn at (1,0), one square from the last row
        "click " + std::to_string(boardPx(5)) + " 5",     // move it to (0,0)
        "wait 1000",     // pawn: 1.0 cells/sec, 1 cell travelled -> 1000ms
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {0, 0}) == "wQ");
}

TEST_CASE("runCommands demonstrates a successful jump interception") {
    GameState st = makeState({"wR bR .", ". . .", ". . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "jump " + std::to_string(boardPx(5)) + " 5",      // wR at (0,0) jumps in place
        "click " + std::to_string(boardPx(105)) + " 5",   // select bR at (0,1)
        "click " + std::to_string(boardPx(5)) + " 5",     // send it toward wR's cell - a legal 1-cell rook move
        "wait 1000",     // rook: 1.0 cells/sec, 1 cell -> 1000ms - same tick as the jump's window
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {0, 0}) == "wR");          // jumper survives, unmoved
    CHECK(tokenAt(st.board, {0, 1}) == EMPTY_TOKEN);   // arriving enemy destroyed mid-air
    CHECK_FALSE(st.arbiter.hasActiveJump());
}

TEST_CASE("runCommands demonstrates a jump landing normally with no interception") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "jump " + std::to_string(boardPx(5)) + " 5",      // wR at (0,0) jumps in place
        "wait 1000",     // no enemy arrives - the jump simply lands
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {0, 0}) == "wR");
    CHECK_FALSE(st.arbiter.hasActiveJump());
}

TEST_CASE("runCommands demonstrates a rejected move attempt against a currently-jumping piece") {
    GameState st = makeState({"wR . .", ". . .", ". . ."});

    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    std::vector<std::string> commands = {
        "jump " + std::to_string(boardPx(5)) + " 5",      // wR at (0,0) starts jumping
        "click " + std::to_string(boardPx(5)) + " 5",     // select the jumping rook itself
        "click " + std::to_string(boardPx(205)) + " 5",   // attempt to move it - must be rejected outright
        "wait 1000",     // the jump lands normally in the meantime
        "print board"
    };
    ScriptRunner::run(commands, st);

    std::cout.rdbuf(old);

    CHECK(captured.str() == formatBoard(st.board));
    CHECK(tokenAt(st.board, {0, 0}) == "wR");   // never moved
    CHECK_FALSE(st.arbiter.hasActiveMotion());
}

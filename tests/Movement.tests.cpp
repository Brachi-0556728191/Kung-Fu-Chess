#include "doctest.h"

#include "../realtime/RealTimeArbiter.hpp"
#include "../rules/RuleEngine.hpp"
#include "../rules/PieceRules.hpp"
#include "../model/Board.hpp"
#include "../model/Position.hpp"
#include "../model/GameState.hpp"
#include "../model/Piece.hpp"
#include "../io/BoardParser.hpp"

namespace {
    PieceMove makeMove(int fromRow, int fromCol, int toRow, int toCol, const std::string& piece) {
        PieceMove m;
        m.from = {fromRow, fromCol};
        m.to   = {toRow, toCol};
        m.startMs = 0;       m.durationMs = 0;
        m.piece = piece;
        return m;
    }
}

TEST_CASE("cellDistance computes chessboard (Chebyshev) distance in cells") {
    CHECK(cellDistance({0, 0}, {3, 4}) == doctest::Approx(4.0));  // max(3,4)
    CHECK(cellDistance({2, 2}, {2, 2}) == doctest::Approx(0.0));
    CHECK(cellDistance({0, 0}, {0, 5}) == doctest::Approx(5.0));  // straight line: unaffected
    CHECK(cellDistance({0, 0}, {3, 3}) == doctest::Approx(3.0));  // pure diagonal: one square per step
}

TEST_CASE("isLegalMove: king moves one square in any direction") {
    Board b = parseBoard({"wK . .", ". . .", ". . ."});
    CHECK(isLegalMove(b, makeMove(0, 0, 1, 1, "wK"), 'K'));
    CHECK(isLegalMove(b, makeMove(0, 0, 0, 1, "wK"), 'K'));
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 2, 2, "wK"), 'K'));
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 0, 0, "wK"), 'K'));
}

TEST_CASE("isLegalMove: rook moves straight and needs a clear path") {
    Board clear = parseBoard({"wR . . .", ". . . .", ". . . .", ". . . ."});
    CHECK(isLegalMove(clear, makeMove(0, 0, 0, 3, "wR"), 'R'));
    CHECK_FALSE(isLegalMove(clear, makeMove(0, 0, 1, 1, "wR"), 'R'));

    Board blocked = parseBoard({"wR wP . ."});
    CHECK_FALSE(isLegalMove(blocked, makeMove(0, 0, 0, 3, "wR"), 'R'));
}

TEST_CASE("isLegalMove: bishop moves diagonally and needs a clear path") {
    Board clear = parseBoard({
        "wB . . .",
        ". . . .",
        ". . . .",
        ". . . ."
    });
    CHECK(isLegalMove(clear, makeMove(0, 0, 3, 3, "wB"), 'B'));
    CHECK_FALSE(isLegalMove(clear, makeMove(0, 0, 3, 2, "wB"), 'B'));

    Board blocked = parseBoard({
        "wB . . .",
        ". wP . .",
        ". . . .",
        ". . . ."
    });
    CHECK_FALSE(isLegalMove(blocked, makeMove(0, 0, 2, 2, "wB"), 'B'));
}

TEST_CASE("isLegalMove: queen moves like rook or bishop but not like a knight") {
    Board b = parseBoard({
        "wQ . . .",
        ". . . .",
        ". . . .",
        ". . . ."
    });
    CHECK(isLegalMove(b, makeMove(0, 0, 0, 3, "wQ"), 'Q'));
    CHECK(isLegalMove(b, makeMove(0, 0, 3, 3, "wQ"), 'Q'));
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 1, 2, "wQ"), 'Q'));
}

TEST_CASE("isLegalMove: knight moves in an L shape and ignores blockers") {
    Board b = parseBoard({
        "wN wP . .",
        "wP wP . .",
        ". . . .",
        ". . . ."
    });
    CHECK(isLegalMove(b, makeMove(0, 0, 2, 1, "wN"), 'N'));
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 1, 1, "wN"), 'N'));
}

TEST_CASE("isLegalMove: pawn advances straight only onto an empty square") {
    Board b = parseBoard({
        ". . .",
        "wP . bP",
        ". . ."
    });
    CHECK(isLegalMove(b, makeMove(1, 0, 0, 0, "wP"), 'P'));
    CHECK_FALSE(isLegalMove(b, makeMove(1, 0, 0, 1, "wP"), 'P'));
    CHECK_FALSE(isLegalMove(b, makeMove(1, 2, 0, 2, "bP"), 'P'));
}

TEST_CASE("isLegalMove: pawn captures diagonally only, never straight") {
    Board b = parseBoard({
        "bP . bP",
        ". wP .",
        ". . ."
    });
    CHECK(isLegalMove(b, makeMove(1, 1, 0, 0, "wP"), 'P'));
    CHECK(isLegalMove(b, makeMove(1, 1, 0, 2, "wP"), 'P'));

    Board straightIntoEnemy = parseBoard({"bP", "wP"});
    CHECK_FALSE(isLegalMove(straightIntoEnemy, makeMove(1, 0, 0, 0, "wP"), 'P'));
}

TEST_CASE("isLegalMove: a pawn may advance two squares from its starting row if the path is clear") {
    Board b = parseBoard({
        ". . .",
        ". . .",
        "wP . .",
        ". . ."
    });
    CHECK(isLegalMove(b, makeMove(2, 0, 0, 0, "wP"), 'P'));
}

TEST_CASE("isLegalMove: a pawn that has already moved may not advance two squares, even with a clear path") {
    Board b = parseBoard({
        ". . .",
        ". . .",
        ". . .",
        "wP . ."
    });
    b.movePiece({3, 0}, {2, 0});   // simulate the pawn's first (one-square) move; hasMoved becomes true

    CHECK_FALSE(isLegalMove(b, makeMove(2, 0, 0, 0, "wP"), 'P'));
}

TEST_CASE("isLegalMove: a two-square pawn move is rejected if the intermediate cell is blocked") {
    Board b = parseBoard({
        ". . .",
        "bN . .",
        "wP . ."
    });
    CHECK_FALSE(isLegalMove(b, makeMove(2, 0, 0, 0, "wP"), 'P'));
}

TEST_CASE("isLegalMove: a two-square pawn move can never capture, even with an otherwise clear path") {
    // The destination is occupied, so isLegalMove routes through
    // pawnCaptureShape (single diagonal step only) instead of the widened
    // pawnShape - a straight two-square shape never matches it, so this is
    // rejected by the shape check itself, before hasMoved or isPathClear are
    // even considered.
    Board b = parseBoard({
        "bN . .",
        ". . .",
        "wP . ."
    });
    CHECK_FALSE(isLegalMove(b, makeMove(2, 0, 0, 0, "wP"), 'P'));
}

TEST_CASE("isLegalMove: rejects a move whose destination is outside board bounds") {
    Board b = parseBoard({"wQ . .", ". . .", ". . ."});
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, -1, 0, "wQ"), 'Q'));  // negative row
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 0, -1, "wQ"), 'Q'));  // negative col
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 3, 0, "wQ"), 'Q'));   // row >= rows()
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 0, 3, "wQ"), 'Q'));   // col >= cols()
}

TEST_CASE("isLegalMove: a piece may never capture its own color") {
    Board b = parseBoard({"wR wP . ."});
    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 0, 1, "wR"), 'R'));
}

TEST_CASE("isLegalMove: an invalid piece char throws PieceError") {
    Board b = parseBoard({". . . .", ". . . .", ". . . .", ". . . ."});
    CHECK_THROWS_AS(isLegalMove(b, makeMove(0, 0, 3, 1, "wX"), 'X'), PieceError);
}

TEST_CASE("isLegalMove returns false when no move rule is registered for a kind") {
    Board b = parseBoard({"wR . .", ". . .", ". . ."});

    auto savedShapes = config::moveShapes;
    config::moveShapes.erase(Kind::Rook);
    struct RestoreShapes {
        decltype(savedShapes)& saved;
        ~RestoreShapes() { config::moveShapes = saved; }
    } restore{savedShapes};

    CHECK_FALSE(isLegalMove(b, makeMove(0, 0, 0, 2, "wR"), 'R'));
}

namespace {
    bool contains(const std::vector<Position>& positions, Position p) {
        for (Position pos : positions) if (pos == p) return true;
        return false;
    }
}

TEST_CASE("legalDestinations: rook on an empty board returns every square on its rank and file") {
    Board b = parseBoard({"wR . . .", ". . . .", ". . . .", ". . . ."});
    Piece rook = *b.pieceAt({0, 0});

    std::vector<Position> dest = legalDestinations(b, rook);

    CHECK(dest.size() == 6);   // 3 along the rank + 3 along the file
    CHECK(contains(dest, {0, 3}));
    CHECK(contains(dest, {3, 0}));
    CHECK_FALSE(contains(dest, {1, 1}));   // never a diagonal
}

TEST_CASE("legalDestinations: rook's slide stops at the first blocker and excludes squares beyond it") {
    Board b = parseBoard({"wR wP . .", ". . . .", ". . . .", ". . . ."});
    Piece rook = *b.pieceAt({0, 0});

    std::vector<Position> dest = legalDestinations(b, rook);

    CHECK_FALSE(contains(dest, {0, 1}));   // occupied by a friendly pawn - not even a capture
    CHECK_FALSE(contains(dest, {0, 2}));   // beyond the blocker - unreachable
    CHECK(contains(dest, {1, 0}));         // the file is still clear
}

TEST_CASE("legalDestinations: a slide includes an enemy-occupied square as a capture but nothing beyond it") {
    Board b = parseBoard({"wR bP . .", ". . . .", ". . . .", ". . . ."});
    Piece rook = *b.pieceAt({0, 0});

    std::vector<Position> dest = legalDestinations(b, rook);

    CHECK(contains(dest, {0, 1}));         // capture the enemy pawn
    CHECK_FALSE(contains(dest, {0, 2}));   // path is blocked at the captured square
}

TEST_CASE("legalDestinations: knight ignores blocking pieces entirely") {
    Board b = parseBoard({
        "wN wP . .",
        "wP wP . .",
        ". . . .",
        ". . . ."
    });
    Piece knight = *b.pieceAt({0, 0});

    std::vector<Position> dest = legalDestinations(b, knight);

    // Both L-shaped targets are reachable even though every square adjacent
    // to the knight is occupied - a knight has no path to clear.
    CHECK(dest.size() == 2);
    CHECK(contains(dest, {1, 2}));
    CHECK(contains(dest, {2, 1}));
}

TEST_CASE("legalDestinations: pawn's two-square opening move disappears once it has moved") {
    Board b = parseBoard({". . .", ". . .", ". . .", "wP . ."});
    Piece pawn = *b.pieceAt({3, 0});

    CHECK(contains(legalDestinations(b, pawn), {1, 0}));   // two squares forward, still available

    b.movePiece({3, 0}, {2, 0});
    Piece movedPawn = *b.pieceAt({2, 0});
    std::vector<Position> destAfter = legalDestinations(b, movedPawn);

    CHECK(contains(destAfter, {1, 0}));    // one square forward: still legal
    CHECK_FALSE(contains(destAfter, {0, 0})); // two squares: no longer legal, hasMoved is now true
}

TEST_CASE("legalDestinations: king returns only its 8 (or fewer, near an edge) adjacent squares") {
    Board b = parseBoard({"wK . .", ". . .", ". . ."});
    Piece king = *b.pieceAt({0, 0});

    std::vector<Position> dest = legalDestinations(b, king);

    CHECK(dest.size() == 3);   // corner square: only 3 of the normal 8 neighbors are in bounds
    CHECK(contains(dest, {0, 1}));
    CHECK(contains(dest, {1, 0}));
    CHECK(contains(dest, {1, 1}));
}

TEST_CASE("legalDestinations: an isolated piece with no legal moves returns an empty list") {
    // A corner knight has only two in-bounds candidate squares, (1,2) and
    // (2,1) - both occupied by friendly pawns here, so neither is reachable
    // (a knight can jump over blockers, but can never land on its own color).
    Board b = parseBoard({"wN . .", ". . wP", ". wP ."});
    Piece knight = *b.pieceAt({0, 0});

    CHECK(legalDestinations(b, knight).empty());
}

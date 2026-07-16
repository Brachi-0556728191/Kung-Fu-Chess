#include "doctest.h"

#include "../rules/PieceRules.hpp"

TEST_CASE("statsFor returns known speeds for standard pieces") {
    // Piece speed is uniform: every kind moves at 1.0 cells/sec (1000ms per cell).
    CHECK(config::statsFor(Kind::Queen).speedCellsPerSec == doctest::Approx(1.0));
    CHECK(config::statsFor(Kind::Rook).speedCellsPerSec == doctest::Approx(1.0));
    CHECK(config::statsFor(Kind::Bishop).speedCellsPerSec == doctest::Approx(1.0));
    CHECK(config::statsFor(Kind::Knight).speedCellsPerSec == doctest::Approx(1.0));
    CHECK(config::statsFor(Kind::King).speedCellsPerSec == doctest::Approx(1.0));
    CHECK(config::statsFor(Kind::Pawn).speedCellsPerSec == doctest::Approx(1.0));
}

TEST_CASE("shape helpers classify king moves") {
    CHECK(config::kingShape(1, 0, 'w'));
    CHECK(config::kingShape(1, 1, 'w'));
    CHECK_FALSE(config::kingShape(0, 0, 'w'));
    CHECK_FALSE(config::kingShape(2, 0, 'w'));
}

TEST_CASE("shape helpers classify rook moves") {
    CHECK(config::rookShape(0, 5, 'w'));
    CHECK(config::rookShape(5, 0, 'w'));
    CHECK_FALSE(config::rookShape(0, 0, 'w'));
    CHECK_FALSE(config::rookShape(2, 2, 'w'));
}

TEST_CASE("shape helpers classify bishop moves") {
    CHECK(config::bishopShape(3, 3, 'w'));
    CHECK(config::bishopShape(-2, 2, 'w'));
    CHECK_FALSE(config::bishopShape(0, 0, 'w'));
    CHECK_FALSE(config::bishopShape(2, 3, 'w'));
}

TEST_CASE("shape helpers classify queen moves as rook or bishop") {
    CHECK(config::queenShape(0, 4, 'w'));
    CHECK(config::queenShape(4, 4, 'w'));
    CHECK_FALSE(config::queenShape(1, 2, 'w'));
}

TEST_CASE("shape helpers classify knight moves") {
    CHECK(config::knightShape(1, 2, 'w'));
    CHECK(config::knightShape(2, 1, 'w'));
    CHECK_FALSE(config::knightShape(1, 1, 'w'));
    CHECK_FALSE(config::knightShape(2, 2, 'w'));
}

TEST_CASE("pawnForwardDir depends on color") {
    CHECK(config::pawnForwardDir(COLOR_WHITE_CHAR) == -1);
    CHECK(config::pawnForwardDir(COLOR_BLACK_CHAR) == 1);
}

TEST_CASE("shape helpers classify pawn moves and captures separately") {
    CHECK(config::pawnShape(-1, 0, 'w'));
    CHECK_FALSE(config::pawnShape(-1, 1, 'w'));
    CHECK(config::pawnCaptureShape(-1, 1, 'w'));
    CHECK(config::pawnCaptureShape(-1, -1, 'w'));
    CHECK_FALSE(config::pawnCaptureShape(-1, 0, 'w'));
}

TEST_CASE("pawnShape accepts a two-square forward move and still rejects sideways/backward moves") {
    CHECK(config::pawnShape(-2, 0, 'w'));           // white: two squares forward
    CHECK(config::pawnShape(2, 0, 'b'));            // black: two squares forward
    CHECK_FALSE(config::pawnShape(-2, 1, 'w'));     // two rows but not straight
    CHECK_FALSE(config::pawnShape(2, 0, 'w'));      // backward for white
    CHECK_FALSE(config::pawnShape(0, 1, 'w'));      // sideways
    CHECK_FALSE(config::pawnShape(-3, 0, 'w'));     // three squares - too far
}

TEST_CASE("moveShapes registers a rule for every standard piece") {
    for (Kind kind : {Kind::King, Kind::Queen, Kind::Rook, Kind::Bishop, Kind::Knight, Kind::Pawn}) {
        CHECK(config::moveShapes.count(kind) == 1);
    }
}

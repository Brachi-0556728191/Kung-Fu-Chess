#include "doctest.h"

#include "../model/Piece.hpp"

#include <string>
#include <vector>

TEST_CASE("pieceFromToken maps all six kinds for white") {
    CHECK(pieceFromToken("wK", {0, 0}, 1).kind == Kind::King);
    CHECK(pieceFromToken("wQ", {0, 0}, 1).kind == Kind::Queen);
    CHECK(pieceFromToken("wR", {0, 0}, 1).kind == Kind::Rook);
    CHECK(pieceFromToken("wB", {0, 0}, 1).kind == Kind::Bishop);
    CHECK(pieceFromToken("wN", {0, 0}, 1).kind == Kind::Knight);
    CHECK(pieceFromToken("wP", {0, 0}, 1).kind == Kind::Pawn);
}

TEST_CASE("pieceFromToken maps all six kinds for black") {
    CHECK(pieceFromToken("bK", {0, 0}, 1).kind == Kind::King);
    CHECK(pieceFromToken("bQ", {0, 0}, 1).kind == Kind::Queen);
    CHECK(pieceFromToken("bR", {0, 0}, 1).kind == Kind::Rook);
    CHECK(pieceFromToken("bB", {0, 0}, 1).kind == Kind::Bishop);
    CHECK(pieceFromToken("bN", {0, 0}, 1).kind == Kind::Knight);
    CHECK(pieceFromToken("bP", {0, 0}, 1).kind == Kind::Pawn);
}

TEST_CASE("pieceFromToken maps color correctly") {
    CHECK(pieceFromToken("wK", {0, 0}, 1).color == Color::White);
    CHECK(pieceFromToken("bK", {0, 0}, 1).color == Color::Black);
}

TEST_CASE("pieceFromToken sets the given position and id, defaults state to Idle") {
    Piece p = pieceFromToken("wQ", Position{2, 5}, 42);
    CHECK(p.id == 42);
    CHECK(p.cell.row == 2);
    CHECK(p.cell.col == 5);
    CHECK(p.state == PieceState::Idle);
}

TEST_CASE("pieceFromToken throws PieceError on an invalid token") {
    CHECK_THROWS_AS(pieceFromToken("xK", {0, 0}, 1), PieceError);
    CHECK_THROWS_AS(pieceFromToken("wX", {0, 0}, 1), PieceError);
    CHECK_THROWS_AS(pieceFromToken("w", {0, 0}, 1), PieceError);
    CHECK_THROWS_AS(pieceFromToken("wKQ", {0, 0}, 1), PieceError);
    CHECK_THROWS_AS(pieceFromToken(".", {0, 0}, 1), PieceError);
    CHECK_THROWS_AS(pieceFromToken("", {0, 0}, 1), PieceError);
}

TEST_CASE("tokenFromPiece is the inverse of pieceFromToken for all kinds and colors") {
    std::vector<std::string> tokens = {
        "wK", "bK", "wQ", "bQ", "wR", "bR",
        "wB", "bB", "wN", "bN", "wP", "bP"
    };
    for (const auto& tok : tokens) {
        Piece p = pieceFromToken(tok, {1, 1}, 7);
        CHECK(tokenFromPiece(p) == tok);
    }
}

TEST_CASE("Position equality behaves as expected") {
    Position a{1, 2};
    Position b{1, 2};
    Position c{2, 1};
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("two pieces built from the same inputs compare equal field by field") {
    Piece p1 = pieceFromToken("wR", {3, 4}, 9);
    Piece p2 = pieceFromToken("wR", {3, 4}, 9);
    CHECK(p1.id == p2.id);
    CHECK(p1.color == p2.color);
    CHECK(p1.kind == p2.kind);
    CHECK(p1.cell == p2.cell);
    CHECK(p1.state == p2.state);
}

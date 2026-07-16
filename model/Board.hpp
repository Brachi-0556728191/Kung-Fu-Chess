#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Piece.hpp"
#include "Position.hpp"

class BoardOperationError : public std::runtime_error {
public:
    explicit BoardOperationError(const std::string& code)
        : std::runtime_error(code), code_(code) {}
    const std::string& code() const { return code_; }
private:
    std::string code_;
};

class Board {
public:
    Board() = default;
    Board(int rows, int cols);

    int rows() const;
    int cols() const;
    bool isInBounds(Position pos) const;

    // pieceAt/isEmpty treat an out-of-bounds position the same as an empty
    // in-bounds one (nullopt / true) so callers don't need to bounds-check
    // before every lookup.
    std::optional<Piece> pieceAt(Position pos) const;
    bool isEmpty(Position pos) const;

    void addPiece(const Piece& piece);           // throws BoardOperationError if out of bounds or occupied
    void removePiece(Position pos);              // no-op if already empty; throws only if out of bounds
    void movePiece(Position from, Position to);  // throws if `from` is empty or either position is out of bounds
    void promoteAt(Position pos, Kind newKind);  // throws BoardOperationError if pos is empty or out of bounds

private:
    std::vector<std::vector<std::optional<Piece>>> cells_;
};

bool isEmpty(const std::string& tok);
char colorOf(const std::string& tok);
char pieceOf(const std::string& tok);

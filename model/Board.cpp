#include "Board.hpp"

Board::Board(int rows, int cols)
    : cells_(rows, std::vector<std::optional<Piece>>(cols)) {}

int Board::rows() const { return (int)cells_.size(); }
int Board::cols() const { return cells_.empty() ? 0 : (int)cells_[0].size(); }

bool Board::isInBounds(Position pos) const {
    return pos.row >= 0 && pos.row < rows() && pos.col >= 0 && pos.col < cols();
}

std::optional<Piece> Board::pieceAt(Position pos) const {
    if (!isInBounds(pos)) return std::nullopt;
    return cells_[pos.row][pos.col];
}

bool Board::isEmpty(Position pos) const {
    return !pieceAt(pos).has_value();
}

void Board::addPiece(const Piece& piece) {
    if (!isInBounds(piece.cell)) throw BoardOperationError("OUT_OF_BOUNDS");
    if (cells_[piece.cell.row][piece.cell.col].has_value()) throw BoardOperationError("CELL_OCCUPIED");
    cells_[piece.cell.row][piece.cell.col] = piece;
}

void Board::removePiece(Position pos) {
    if (!isInBounds(pos)) throw BoardOperationError("OUT_OF_BOUNDS");
    cells_[pos.row][pos.col].reset();
}

void Board::movePiece(Position from, Position to) {
    if (!isInBounds(from) || !isInBounds(to)) throw BoardOperationError("OUT_OF_BOUNDS");

    std::optional<Piece>& src = cells_[from.row][from.col];
    if (!src.has_value()) throw BoardOperationError("SOURCE_EMPTY");

    Piece moved = *src;
    moved.cell = to;
    moved.hasMoved = true;
    cells_[to.row][to.col] = moved;
    src.reset();
}

void Board::promoteAt(Position pos, Kind newKind) {
    if (!isInBounds(pos)) throw BoardOperationError("OUT_OF_BOUNDS");

    std::optional<Piece>& cell = cells_[pos.row][pos.col];
    if (!cell.has_value()) throw BoardOperationError("CELL_EMPTY");

    cell->kind = newKind;
}

bool isEmpty(const std::string& tok) { return tok == EMPTY_TOKEN; }
char colorOf(const std::string& tok) { return tok[0]; }
char pieceOf(const std::string& tok) { return tok[1]; }

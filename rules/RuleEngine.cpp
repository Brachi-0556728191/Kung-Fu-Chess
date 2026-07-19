#include "RuleEngine.hpp"

#include <optional>

#include "PieceRules.hpp"

namespace {

bool isLegalDestination(const Board& board, Position from, Position to, Kind kind, char color, bool hasMoved) {
    if (!board.isInBounds(to)) return false;

    auto it = config::moveShapes.find(kind);
    if (it == config::moveShapes.end()) return false;

    const config::MoveRule& rule = it->second;

    std::optional<Piece> destination = board.pieceAt(to);
    if (destination && colorToChar(destination->color) == color) return false;

    bool isCapture = destination.has_value();
    const config::MoveShapeFn& shape = (isCapture && rule.captureShape) ? rule.captureShape : rule.shape;

    int dRow = to.row - from.row;
    int dCol = to.col - from.col;
    if (!shape(dRow, dCol, color)) return false;

    if (kind == Kind::Pawn && dCol == 0 && (dRow == 2 || dRow == -2) && hasMoved) return false;

    if (rule.slides && !isPathClear(board, from, to)) return false;

    return true;
}

}  // namespace

bool isLegalMove(const Board& board, const PieceMove& move, char piece) {
    Kind kind = kindFromChar(piece);   // throws PieceError for an invalid char - unchanged behavior
    char color = move.piece[0];

    std::optional<Piece> movingPiece = board.pieceAt(move.from);
    bool hasMoved = movingPiece && movingPiece->hasMoved;

    return isLegalDestination(board, move.from, move.to, kind, color, hasMoved);
}

std::vector<Position> legalDestinations(const Board& board, const Piece& piece) {
    std::vector<Position> destinations;
    char color = colorToChar(piece.color);

    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            Position to{r, c};
            if (isLegalDestination(board, piece.cell, to, piece.kind, color, piece.hasMoved)) {
                destinations.push_back(to);
            }
        }
    }
    return destinations;
}

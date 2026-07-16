#include "RuleEngine.hpp"

#include <optional>

#include "PieceRules.hpp"

bool isLegalMove(const Board& board, const PieceMove& move, char piece) {
    if (!board.isInBounds(move.to)) return false;

    Kind kind = kindFromChar(piece);
    auto it = config::moveShapes.find(kind);
    if (it == config::moveShapes.end()) return false;  

    const config::MoveRule& rule = it->second;
    char color = move.piece[0];

    std::optional<Piece> destination = board.pieceAt(move.to);
    if (destination && colorToChar(destination->color) == color) return false;

    bool isCapture = destination.has_value();
    const config::MoveShapeFn& shape = (isCapture && rule.captureShape) ? rule.captureShape : rule.shape;

    int dRow = move.to.row - move.from.row;
    int dCol = move.to.col - move.from.col;
    if (!shape(dRow, dCol, color)) return false;

    if (kind == Kind::Pawn && dCol == 0 && (dRow == 2 || dRow == -2)) {
        std::optional<Piece> movingPiece = board.pieceAt(move.from);
        if (movingPiece && movingPiece->hasMoved) return false;
    }

    if (rule.slides && !isPathClear(board, move.from, move.to))
        return false;

    return true;
}

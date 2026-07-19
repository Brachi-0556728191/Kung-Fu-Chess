#include "PieceRules.hpp"

#include <cmath>

int sign(int v) { return (v > 0) - (v < 0); }

bool isPathClear(const Board& board, Position from, Position to) {
    int stepRow = sign(to.row - from.row);
    int stepCol = sign(to.col - from.col);

    int r = from.row + stepRow, c = from.col + stepCol;
    while (r != to.row || c != to.col) {
        if (!board.isEmpty(Position{r, c})) return false;
        r += stepRow;
        c += stepCol;
    }
    return true;
}

namespace config {

    PieceStats statsFor(Kind kind) {
        // Rest durations are placeholders - same for every kind for now,
        // mirroring how assets/Pieces/*/states/*/config.json currently gives
        // every kind identical animation timings too. Tune per-kind later.
        switch (kind) {
            case Kind::Queen:  return {1.0, 800, 1500};
            case Kind::Rook:   return {1.0, 800, 1500};
            case Kind::Knight: return {1.0, 800, 1500};
            case Kind::King:   return {1.0, 800, 1500};
            case Kind::Bishop: return {1.0, 800, 1500};
            case Kind::Pawn:   return {1.0, 800, 1500};
            default:           return {1.0, 800, 1500};
        }
    }

    int pointValueFor(Kind kind) {
        switch (kind) {
            case Kind::Pawn:   return 1;
            case Kind::Knight: return 3;
            case Kind::Bishop: return 3;
            case Kind::Rook:   return 5;
            case Kind::Queen:  return 9;
            case Kind::King:   return 0;
        }
        throw PieceError("INVALID_PIECE_TOKEN");
    }

    bool kingShape(int dRow, int dCol, char /*color*/) {
        return (dRow != 0 || dCol != 0) && std::abs(dRow) <= 1 && std::abs(dCol) <= 1;
    }
    bool rookShape(int dRow, int dCol, char /*color*/) {
        return (dRow == 0) != (dCol == 0);
    }
    bool bishopShape(int dRow, int dCol, char /*color*/) {
        return dRow != 0 && std::abs(dRow) == std::abs(dCol);
    }
    bool queenShape(int dRow, int dCol, char /*color*/) {
        return rookShape(dRow, dCol, ' ') || bishopShape(dRow, dCol, ' ');
    }
    bool knightShape(int dRow, int dCol, char /*color*/) {
        int r = std::abs(dRow), c = std::abs(dCol);
        return (r == 1 && c == 2) || (r == 2 && c == 1);
    }
    int pawnForwardDir(char color) {
        return (color == COLOR_WHITE_CHAR) ? -1 : 1;
    }
    bool pawnShape(int dRow, int dCol, char color) {
        int dir = pawnForwardDir(color);
        return dCol == 0 && (dRow == dir || dRow == 2 * dir);
    }
    bool pawnCaptureShape(int dRow, int dCol, char color) {
        return std::abs(dCol) == 1 && dRow == pawnForwardDir(color);
    }

    bool shouldPromote(const Piece& piece, Position to, int boardRows) {
        if (piece.kind != Kind::Pawn) return false;
        int lastRow = (pawnForwardDir(colorToChar(piece.color)) < 0) ? 0 : boardRows - 1;
        return to.row == lastRow;
    }

    Kind promotionTarget(const Piece& /*piece*/) {
        // currently always promotes to Queen; extension point for player choice
        return Kind::Queen;
    }

    std::map<Kind, MoveRule> moveShapes = {
        {Kind::King,   {kingShape,   false}},
        {Kind::Queen,  {queenShape,  true }},
        {Kind::Rook,   {rookShape,   true }},
        {Kind::Bishop, {bishopShape, true }},
        {Kind::Knight, {knightShape, false}},
        {Kind::Pawn,   {pawnShape,   true,  pawnCaptureShape}},
    };
}

#pragma once

#include <functional>
#include <map>

#include "../model/Board.hpp"
#include "../model/Piece.hpp"
#include "../model/Position.hpp"

int sign(int v);

bool isPathClear(const Board& board, Position from, Position to);

namespace config {

    struct PieceStats {
        double speedCellsPerSec;
        long   shortRestMs;   // cooldown after a jump (RestKind::Short)
        long   longRestMs;    // cooldown after a completed move (RestKind::Long)
    };

    PieceStats statsFor(Kind kind);


    int pointValueFor(Kind kind);

    using MoveShapeFn = std::function<bool(int dRow, int dCol, char color)>;

    bool kingShape(int dRow, int dCol, char color);
    bool rookShape(int dRow, int dCol, char color);
    bool bishopShape(int dRow, int dCol, char color);
    bool queenShape(int dRow, int dCol, char color);
    bool knightShape(int dRow, int dCol, char color);
    int  pawnForwardDir(char color);
    bool pawnShape(int dRow, int dCol, char color);
    bool pawnCaptureShape(int dRow, int dCol, char color);

    bool shouldPromote(const Piece& piece, Position to, int boardRows);
    Kind promotionTarget(const Piece& piece);

    struct MoveRule {
        MoveShapeFn shape;
        bool        slides;
        MoveShapeFn captureShape = nullptr;
    };

    extern std::map<Kind, MoveRule> moveShapes;
}

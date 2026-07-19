#pragma once

#include "Piece.hpp"
#include "Position.hpp"

struct MoveRecord {
    Color    color;
    Kind     kind;
    Position from;
    Position to;
    bool     captured = false;
};

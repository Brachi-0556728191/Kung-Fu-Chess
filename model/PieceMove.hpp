#pragma once

#include <string>

#include "Position.hpp"

struct PieceMove {
    Position    from;
    Position    to;
    long        startMs;
    long        durationMs;
    std::string piece;
};

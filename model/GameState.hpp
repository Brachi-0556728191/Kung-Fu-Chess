#pragma once

#include "Board.hpp"
#include "Position.hpp"
#include "PieceMove.hpp"
#include "../realtime/RealTimeArbiter.hpp"

struct Selection {
    bool     active = false;
    Position cell = {0, 0};
    long     selectedAtMs = 0;

    void clear() { *this = Selection{}; }
};

struct GameState {
    Board           board;
    long            elapsedMs = 0;
    Selection       selection;
    RealTimeArbiter arbiter;   // RealTimeArbiter owns the active-motion state;
                               // GameState merely holds an instance of it.
    bool            gameOver = false;
};

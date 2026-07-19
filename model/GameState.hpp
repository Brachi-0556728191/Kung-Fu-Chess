#pragma once

#include <vector>

#include "Board.hpp"
#include "Position.hpp"
#include "PieceMove.hpp"
#include "MoveRecord.hpp"
#include "Score.hpp"
#include "GameOverState.hpp"
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
    RealTimeArbiter arbiter;
    GameOverState   gameOver;

    std::vector<MoveRecord> moveHistory;
    Score                   score;
};

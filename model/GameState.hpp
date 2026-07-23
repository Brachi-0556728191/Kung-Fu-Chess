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

struct SelectionState {
    Selection white;
    Selection black;

    Selection&       forColor(Color c)       { return c == Color::White ? white : black; }
    const Selection& forColor(Color c) const { return c == Color::White ? white : black; }
};

struct GameState {
    Board           board;
    long            elapsedMs = 0;
    Selection       selection;
    SelectionState  selections;
    RealTimeArbiter arbiter;
    GameOverState   gameOver;

    std::vector<MoveRecord> moveHistory;
    Score                   score;
};

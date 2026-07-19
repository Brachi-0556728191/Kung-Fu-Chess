#pragma once

#include "Piece.hpp"


enum class GameOverReason { KingCaptured };

struct GameOverState {
    bool           isOver = false;
    Color          winner = Color::White;   
    GameOverReason reason = GameOverReason::KingCaptured;

    operator bool() const { return isOver; }
};

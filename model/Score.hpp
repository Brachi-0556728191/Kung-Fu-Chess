#pragma once

#include "Piece.hpp"

    int white = 0;
    int black = 0;

    void add(Color color, int points) {
        (color == Color::White ? white : black) += points;
    }
};

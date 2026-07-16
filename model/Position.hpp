#pragma once

struct Position {
    int row, col;
    bool operator==(const Position& other) const { return row == other.row && col == other.col; }
    bool operator!=(const Position& other) const { return !(*this == other); }
};

double cellDistance(Position a, Position b);

#include "Position.hpp"

#include <algorithm>
#include <cmath>

double cellDistance(Position a, Position b) {
    double dr = std::abs(b.row - a.row);
    double dc = std::abs(b.col - a.col);
    return std::max(dr, dc);
}

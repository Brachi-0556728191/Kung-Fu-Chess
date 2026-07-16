#pragma once

#include <optional>

#include "../model/Board.hpp"
#include "../model/Position.hpp"

std::optional<Position> pixelToCell(int x, int y, const Board& board);

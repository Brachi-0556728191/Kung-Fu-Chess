#pragma once

#include "../model/Board.hpp"
#include "../model/GameState.hpp"

bool isLegalMove(const Board& board, const PieceMove& move, char piece);

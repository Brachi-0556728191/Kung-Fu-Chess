#pragma once

#include <vector>

#include "../model/Board.hpp"
#include "../model/GameState.hpp"

bool isLegalMove(const Board& board, const PieceMove& move, char piece);

std::vector<Position> legalDestinations(const Board& board, const Piece& piece);

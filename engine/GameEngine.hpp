#pragma once

#include "../model/GameState.hpp"

void sendMove(GameState& st, int toRow, int toCol);

bool startJump(GameState& st, Position cell);

void handleWait(GameState& st, long ms);

#pragma once

#include <mutex>

#include "model/GameState.hpp"

struct GameInstance {
    GameState  state;
    std::mutex mutex;
};

#pragma once

#include <string>
#include <vector>

#include "../model/GameState.hpp"

namespace ScriptRunner {

    void run(const std::vector<std::string>& commands, GameState& st);

}

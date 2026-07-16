#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "../model/GameState.hpp"

namespace view {

cv::Mat renderBoard(const GameState& state);

void showBoard(const GameState& state, const std::string& windowName = "Chess");

}  // namespace view

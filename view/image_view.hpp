#pragma once

#include <string>

#include <opencv2/core.hpp>

#include "../model/Piece.hpp"
#include "AnimationState.hpp"

namespace view {

std::string pieceSpritePath(Kind kind, Color color, AnimationState state, int frameIndex);

cv::Mat loadPieceSprite(Kind kind, Color color, AnimationState state, int frameIndex);

AnimationTiming loadAnimationTiming(Kind kind, Color color, AnimationState state);

}  // namespace view

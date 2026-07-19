#pragma once

#include <optional>
#include <string>

#include "../model/Piece.hpp"
#include "../realtime/RealTimeArbiter.hpp"

namespace view {

enum class AnimationState { Idle, Move, Jump, ShortRest, LongRest };

constexpr int ANIMATION_FRAME_COUNT = 5;  // every state ships exactly 5 sprite frames

std::string animationStateFolder(AnimationState state);

struct AnimationTiming {
    double framesPerSec;
    bool   isLoop;
};

struct AnimationInfo {
    AnimationState state;
    long           stateStartMs;
};

AnimationInfo resolveAnimationState(const Piece& piece, const RealTimeArbiter& arbiter);

// 0-based index into the state's 5 sprite frames.
int animationFrameIndex(long elapsedMs, const AnimationInfo& info, const AnimationTiming& timing);

// cell time of Cooldown 
std::optional<double> restRemainingFraction(const Piece& piece, const RealTimeArbiter& arbiter, long elapsedMs);

}  // namespace view

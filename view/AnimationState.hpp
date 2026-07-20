#pragma once

#include <optional>
#include <string>

#include "../model/Piece.hpp"
#include "../model/PieceJump.hpp"
#include "../model/PieceMove.hpp"
#include "../model/PieceRest.hpp"

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


AnimationInfo resolveAnimationState(const Piece& piece,
                                     const std::optional<PieceMove>& activeMove,
                                     const std::optional<PieceJump>& activeJump,
                                     const std::optional<PieceRest>& activeRest);

// 0-based index into the state's 5 sprite frames.
int animationFrameIndex(long elapsedMs, const AnimationInfo& info, const AnimationTiming& timing);


std::optional<double> restRemainingFraction(const std::optional<PieceRest>& activeRest, long elapsedMs);

}  // namespace view

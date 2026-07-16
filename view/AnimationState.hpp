#pragma once

#include <string>

#include "../model/Piece.hpp"
#include "../realtime/RealTimeArbiter.hpp"

namespace view {

// One enum value per assets/Pieces/<kind><color>/states/<folder> directory.
// Adding a 6th state later means: add the asset folder, add an enum value,
// add one line to animationStateFolder(), and (if some game event should
// enter/exit it) a few lines in resolveAnimationState().
enum class AnimationState { Idle, Move, Jump, ShortRest, LongRest };

constexpr int ANIMATION_FRAME_COUNT = 5;  // every state ships exactly 5 sprite frames

std::string animationStateFolder(AnimationState state);

struct AnimationTiming {
    double framesPerSec;
    bool   isLoop;
};

// Which state a given piece is in right now, and when it entered that
// state (used as the phase reference for frame cycling). This is derived
// entirely from the arbiter's existing bookkeeping - it introduces no new
// per-piece storage of its own, so there is nothing for it to fall out of
// sync with.
struct AnimationInfo {
    AnimationState state;
    long           stateStartMs;
};

AnimationInfo resolveAnimationState(const Piece& piece, const RealTimeArbiter& arbiter);

// 0-based index into the state's 5 sprite frames.
int animationFrameIndex(long elapsedMs, const AnimationInfo& info, const AnimationTiming& timing);

}  // namespace view

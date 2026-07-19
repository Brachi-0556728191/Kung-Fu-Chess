#include "AnimationState.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace view {

std::string animationStateFolder(AnimationState state) {
    switch (state) {
        case AnimationState::Idle:      return "idle";
        case AnimationState::Move:      return "move";
        case AnimationState::Jump:      return "jump";
        case AnimationState::ShortRest: return "short_rest";
        case AnimationState::LongRest:  return "long_rest";
    }
    throw std::runtime_error("Unhandled AnimationState");
}

namespace {

// This one genuinely is a key -> value relationship, so it gets a real map.
const std::map<RestKind, AnimationState> REST_TO_ANIMATION_STATE = {
    {RestKind::Short, AnimationState::ShortRest},
    {RestKind::Long,  AnimationState::LongRest},
};


using StateCheck = std::optional<AnimationInfo> (*)(const Piece&, const RealTimeArbiter&);

std::optional<AnimationInfo> checkMove(const Piece& piece, const RealTimeArbiter& arbiter) {
    auto move = arbiter.activeMoveFor(piece.cell);
    if (move) return AnimationInfo{AnimationState::Move, move->startMs};
    return std::nullopt;
}

std::optional<AnimationInfo> checkJump(const Piece& piece, const RealTimeArbiter& arbiter) {
    auto jump = arbiter.activeJump();
    if (jump && jump->cell == piece.cell) return AnimationInfo{AnimationState::Jump, jump->startMs};
    return std::nullopt;
}

std::optional<AnimationInfo> checkRest(const Piece& piece, const RealTimeArbiter& arbiter) {
    auto rest = arbiter.activeRest(piece.id);
    if (!rest) return std::nullopt;
    return AnimationInfo{REST_TO_ANIMATION_STATE.at(rest->kind), rest->startMs};
}

const StateCheck STATE_CHECKS[] = {checkMove, checkJump, checkRest};

}  // namespace

AnimationInfo resolveAnimationState(const Piece& piece, const RealTimeArbiter& arbiter) {
    for (StateCheck check : STATE_CHECKS) {
        if (auto info = check(piece, arbiter)) return *info;
    }

    return {AnimationState::Idle, 0};
}

int animationFrameIndex(long elapsedMs, const AnimationInfo& info, const AnimationTiming& timing) {
    long dtMs = std::max<long>(0, elapsedMs - info.stateStartMs);
    int frame = static_cast<int>(static_cast<double>(dtMs) * timing.framesPerSec / 1000.0);
    return timing.isLoop ? (frame % ANIMATION_FRAME_COUNT)
                          : std::min(frame, ANIMATION_FRAME_COUNT - 1);
}

std::optional<double> restRemainingFraction(const Piece& piece, const RealTimeArbiter& arbiter, long elapsedMs) {
    auto rest = arbiter.activeRest(piece.id);
    if (!rest) return std::nullopt;

    long totalMs = rest->untilMs - rest->startMs;
    if (totalMs <= 0) return 0.0;   // degenerate (zero-length) cooldown: already done

    double remaining = double(rest->untilMs - elapsedMs) / double(totalMs);
    return std::clamp(remaining, 0.0, 1.0);
}

}  // namespace view

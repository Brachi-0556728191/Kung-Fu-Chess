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


using StateCheck = std::optional<AnimationInfo> (*)(const Piece&,
                                                      const std::optional<PieceMove>&,
                                                      const std::optional<PieceJump>&,
                                                      const std::optional<PieceRest>&);

std::optional<AnimationInfo> checkMove(const Piece&, const std::optional<PieceMove>& move,
                                        const std::optional<PieceJump>&, const std::optional<PieceRest>&) {
    if (move) return AnimationInfo{AnimationState::Move, move->startMs};
    return std::nullopt;
}

std::optional<AnimationInfo> checkJump(const Piece& piece, const std::optional<PieceMove>&,
                                        const std::optional<PieceJump>& jump, const std::optional<PieceRest>&) {
    if (jump && jump->cell == piece.cell) return AnimationInfo{AnimationState::Jump, jump->startMs};
    return std::nullopt;
}

std::optional<AnimationInfo> checkRest(const Piece&, const std::optional<PieceMove>&,
                                        const std::optional<PieceJump>&, const std::optional<PieceRest>& rest) {
    if (!rest) return std::nullopt;
    return AnimationInfo{REST_TO_ANIMATION_STATE.at(rest->kind), rest->startMs};
}


const StateCheck STATE_CHECKS[] = {checkMove, checkJump, checkRest};

}  // namespace

AnimationInfo resolveAnimationState(const Piece& piece,
                                     const std::optional<PieceMove>& activeMove,
                                     const std::optional<PieceJump>& activeJump,
                                     const std::optional<PieceRest>& activeRest) {
    for (StateCheck check : STATE_CHECKS) {
        if (auto info = check(piece, activeMove, activeJump, activeRest)) return *info;
    }

    return {AnimationState::Idle, 0};
}

int animationFrameIndex(long elapsedMs, const AnimationInfo& info, const AnimationTiming& timing) {
    long dtMs = std::max<long>(0, elapsedMs - info.stateStartMs);
    int frame = static_cast<int>(static_cast<double>(dtMs) * timing.framesPerSec / 1000.0);
    return timing.isLoop ? (frame % ANIMATION_FRAME_COUNT)
                          : std::min(frame, ANIMATION_FRAME_COUNT - 1);
}

std::optional<double> restRemainingFraction(const std::optional<PieceRest>& activeRest, long elapsedMs) {
    if (!activeRest) return std::nullopt;

    long totalMs = activeRest->untilMs - activeRest->startMs;
    if (totalMs <= 0) return 0.0;   // degenerate (zero-length) cooldown: already done

    double remaining = double(activeRest->untilMs - elapsedMs) / double(totalMs);
    return std::clamp(remaining, 0.0, 1.0);
}

}  // namespace view

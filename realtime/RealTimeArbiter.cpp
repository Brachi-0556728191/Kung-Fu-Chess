#include "RealTimeArbiter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../model/Board.hpp"
#include "../rules/PieceRules.hpp"

namespace {
    constexpr long JUMP_DURATION_MS = 1000;
}

bool RealTimeArbiter::hasActiveMotion() const {
    return !activeMoves_.empty();
}

std::optional<PieceMove> RealTimeArbiter::activeMoveFor(Position pos) const {
    for (const PieceMove& m : activeMoves_)
        if (m.from == pos) return m;
    return std::nullopt;
}

void RealTimeArbiter::startMotion(const PieceMove& move) {
    if (isPieceCurrentlyMoving(move.from)) throw RealTimeArbiterError("MOTION_ALREADY_ACTIVE");
    activeMoves_.push_back(move);
}

bool RealTimeArbiter::hasActiveJump() const {
    return activeJump_.has_value();
}

bool RealTimeArbiter::isPieceCurrentlyMoving(Position pos) const {
    return activeMoveFor(pos).has_value();
}

bool RealTimeArbiter::isPieceCurrentlyJumping(Position pos) const {
    return activeJump_.has_value() && activeJump_->cell == pos;
}

void RealTimeArbiter::startJump(Position cell, long startMs) {
    if (activeJump_.has_value()) throw RealTimeArbiterError("JUMP_ALREADY_ACTIVE");
    if (isPieceCurrentlyMoving(cell)) throw RealTimeArbiterError("PIECE_ALREADY_MOVING");
    activeJump_ = PieceJump{cell, startMs};
}

std::optional<PieceJump> RealTimeArbiter::activeJump() const {
    return activeJump_;
}

std::optional<PieceRest> RealTimeArbiter::activeRest(int pieceId) const {
    for (const PieceRest& rest : resting_)
        if (rest.pieceId == pieceId) return rest;
    return std::nullopt;
}

void RealTimeArbiter::startRest(int pieceId, RestKind kind, long startMs, long untilMs) {
    resting_.push_back(PieceRest{pieceId, kind, startMs, untilMs});
}


std::vector<ArrivalEvent> RealTimeArbiter::resolveMoves(long elapsedMs, Board& board) {
    std::vector<PieceMove> due;
    std::vector<PieceMove> stillFlying;
    for (const PieceMove& m : activeMoves_) {
        if (elapsedMs >= m.startMs + m.durationMs) due.push_back(m);
        else stillFlying.push_back(m);
    }

    std::sort(due.begin(), due.end(), [](const PieceMove& a, const PieceMove& b) {
        return (a.startMs + a.durationMs) < (b.startMs + b.durationMs);
    });

    std::vector<ArrivalEvent> events;
    for (const PieceMove& m : due) {
        ArrivalEvent event;
        std::optional<Piece> movingPiece = board.pieceAt(m.from);

        if (movingPiece) {
            bool intercepted = false;

            if (activeJump_ && activeJump_->cell == m.to) {
                std::optional<Piece> jumpingPiece = board.pieceAt(activeJump_->cell);
                if (jumpingPiece && jumpingPiece->color != movingPiece->color) {
                    // Interception: the arriving enemy is destroyed mid-air,
                    // never placed at the destination; the jumper is untouched.
                    Piece destroyed = *movingPiece;
                    destroyed.state = PieceState::Captured;
                    event.capturedPiece = destroyed;
                    event.pieceArrived = true;
                    board.removePiece(m.from);

                    startRest(jumpingPiece->id, RestKind::Short, elapsedMs,
                              elapsedMs + config::statsFor(jumpingPiece->kind).shortRestMs);
                    activeJump_.reset();   // the jump concluded successfully
                    intercepted = true;
                }
            }

            if (!intercepted) {
                std::optional<Piece> destPiece = board.pieceAt(m.to);
                if (!destPiece || destPiece->color != movingPiece->color) {
                    if (destPiece) {
                        destPiece->state = PieceState::Captured;
                        event.capturedPiece = destPiece;
                    }
                    board.movePiece(m.from, m.to);

                    if (config::shouldPromote(*movingPiece, m.to, board.rows())) {
                        board.promoteAt(m.to, config::promotionTarget(*movingPiece));
                    }

                    startRest(movingPiece->id, RestKind::Long, elapsedMs,
                              elapsedMs + config::statsFor(movingPiece->kind).longRestMs);

                    event.pieceArrived = true;
                }
                // else: friendly-blocked - event.pieceArrived stays false,
                // board unchanged, and activeJump_ (if any) is left untouched.
            }
        }

        events.push_back(event);
    }

    activeMoves_ = std::move(stillFlying);
    return events;
}

std::vector<ArrivalEvent> RealTimeArbiter::advanceTime(long elapsedMs, Board& board) {
    std::vector<ArrivalEvent> events = resolveMoves(elapsedMs, board);

    // Separately, resolve the jump's own timeout - only reached here if it
    // wasn't already concluded by an interception inside resolveMoves above
    // (in which case activeJump_ is already reset and this is a no-op).
    if (activeJump_ && elapsedMs >= activeJump_->startMs + JUMP_DURATION_MS) {
        std::optional<Piece> jumper = board.pieceAt(activeJump_->cell);
        if (jumper) {
            startRest(jumper->id, RestKind::Short, elapsedMs,
                      elapsedMs + config::statsFor(jumper->kind).shortRestMs);
        }
        activeJump_.reset();   // lands normally: zero board change
    }

    resting_.erase(
        std::remove_if(resting_.begin(), resting_.end(),
                        [elapsedMs](const PieceRest& r) { return elapsedMs >= r.untilMs; }),
        resting_.end());

    return events;
}

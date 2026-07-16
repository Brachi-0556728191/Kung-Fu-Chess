#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../model/Board.hpp"
#include "../model/Piece.hpp"
#include "../model/PieceJump.hpp"
#include "../model/PieceMove.hpp"
#include "../model/PieceRest.hpp"
#include "../model/Position.hpp"

class RealTimeArbiterError : public std::runtime_error {
public:
    explicit RealTimeArbiterError(const std::string& code)
        : std::runtime_error(code), code_(code) {}
    const std::string& code() const { return code_; }
private:
    std::string code_;
};

struct ArrivalEvent {
    bool pieceArrived = false;
    std::optional<Piece> capturedPiece;
};

class RealTimeArbiter {
public:
    bool hasActiveMotion() const;
    void startMotion(const PieceMove& move);

    // Advances the clock and settles every motion whose arrival time has
    // passed. Several pieces can be independently in flight at once, so this
    // returns one ArrivalEvent per motion that settled this call (in arrival
    // order), not just zero-or-one.
    std::vector<ArrivalEvent> advanceTime(long elapsedMs, Board& board);

    // The motion belonging to the piece currently sitting at `pos`, if any.
    std::optional<PieceMove> activeMoveFor(Position pos) const;

    bool hasActiveJump() const;
    bool isPieceCurrentlyMoving(Position pos) const;
    bool isPieceCurrentlyJumping(Position pos) const;
    void startJump(Position cell, long startMs);
    std::optional<PieceJump> activeJump() const;

    // A resting piece is one cooling down after a move (RestKind::Long) or
    // a jump (RestKind::Short); GameEngine consults this to refuse further
    // moves/jumps, and the view consults it to pick the rest animation.
    std::optional<PieceRest> activeRest(int pieceId) const;

private:
    std::vector<ArrivalEvent> resolveMoves(long elapsedMs, Board& board);
    void startRest(int pieceId, RestKind kind, long startMs, long untilMs);

    std::vector<PieceMove>   activeMoves_;
    std::optional<PieceJump> activeJump_;
    std::vector<PieceRest>   resting_;
};

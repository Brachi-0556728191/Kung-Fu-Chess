#include "GameEngine.hpp"

#include <optional>
#include <vector>

#include "../model/Board.hpp"
#include "../model/Position.hpp"
#include "../rules/PieceRules.hpp"
#include "../rules/RuleEngine.hpp"
#include "../realtime/RealTimeArbiter.hpp"

void sendMove(GameState& st, int toRow, int toCol) {
    Selection& sel = st.selection;

    if (st.gameOver) {
        sel.clear();
        return;
    }
    if (st.arbiter.isPieceCurrentlyMoving(sel.cell)) {
        sel.clear();
        return;
    }
    if (st.arbiter.isPieceCurrentlyJumping(sel.cell)) {
        sel.clear();
        return;
    }

    std::optional<Piece> movingPiece = st.board.pieceAt(sel.cell);
    if (!movingPiece) {
        sel.clear();
        return;
    }
    if (st.arbiter.activeRest(movingPiece->id)) {
        sel.clear();
        return;
    }
    const std::string selected = tokenFromPiece(*movingPiece);

    PieceMove m;
    m.from = sel.cell;
    m.to   = Position{toRow, toCol};
    m.startMs = st.elapsedMs;
    m.piece   = selected;

    char piece = pieceOf(selected);
    Kind kind = kindFromChar(piece);
    double speed = config::statsFor(kind).speedCellsPerSec;
    double dist  = cellDistance(m.from, m.to);
    m.durationMs = (speed > 0.0) ? (long)(dist / speed * 1000.0) : 0;

    if (isLegalMove(st.board, m, piece)) {
        st.arbiter.startMotion(m);
    }
    sel.clear();
}

bool startJump(GameState& st, Position cell) {
    if (st.gameOver) return false;

    std::optional<Piece> piece = st.board.pieceAt(cell);
    if (!piece) return false;
    if (st.arbiter.activeRest(piece->id)) return false;

    try {
        st.arbiter.startJump(cell, st.elapsedMs);
        return true;
    } catch (const RealTimeArbiterError&) {
        // Rejected - same treatment as any other illegal/blocked move
        // attempt elsewhere in this codebase: no exception escapes
        // GameEngine, caller learns via the bool return instead.
        return false;
    }
}

void handleWait(GameState& st, long ms) {
    st.elapsedMs += ms;
    std::vector<ArrivalEvent> events = st.arbiter.advanceTime(st.elapsedMs, st.board);
    for (const ArrivalEvent& event : events) {
        if (event.capturedPiece && event.capturedPiece->kind == Kind::King) {
            st.gameOver = true;
        }
    }
}

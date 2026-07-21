#pragma once

#include <nlohmann/json.hpp>

#include <optional>

#include "messages.h"

namespace nlohmann {
template <typename T>
struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& opt) {
        if (opt.has_value()) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }
    static void from_json(const json& j, std::optional<T>& opt) {
        if (j.is_null()) {
            opt.reset();
        } else {
            opt = j.get<T>();
        }
    }
};
}  // namespace nlohmann

// ---------------------------------------------------------------------------
// Model types (global namespace, per model/*.hpp) - messages.h embeds these
// directly, so ADL needs to find their to_json/from_json here too.
// ---------------------------------------------------------------------------

NLOHMANN_JSON_SERIALIZE_ENUM(Color, {
    {Color::White, "white"},
    {Color::Black, "black"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(Kind, {
    {Kind::King, "king"},
    {Kind::Queen, "queen"},
    {Kind::Rook, "rook"},
    {Kind::Bishop, "bishop"},
    {Kind::Knight, "knight"},
    {Kind::Pawn, "pawn"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(PieceState, {
    {PieceState::Idle, "idle"},
    {PieceState::Moving, "moving"},
    {PieceState::Captured, "captured"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(RestKind, {
    {RestKind::Short, "short"},
    {RestKind::Long, "long"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(GameOverReason, {
    {GameOverReason::KingCaptured, "king_captured"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, row, col)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PieceMove, from, to, startMs, durationMs, piece)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PieceRest, pieceId, kind, startMs, untilMs)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PieceJump, cell, startMs)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MoveRecord, color, kind, from, to, captured)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Score, white, black)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameOverState, isOver, winner, reason)

// ---------------------------------------------------------------------------
// net:: message types
// ---------------------------------------------------------------------------

namespace net {

NLOHMANN_JSON_SERIALIZE_ENUM(MessageType, {
    {MessageType::Login, "login"},
    {MessageType::Play, "play"},
    {MessageType::RoomJoin, "room_join"},
    {MessageType::RoomCreate, "room_create"},
    {MessageType::Click, "click"},
    {MessageType::Jump, "jump"},
    {MessageType::LoginOk, "login_ok"},
    {MessageType::LoginFail, "login_fail"},
    {MessageType::MatchFound, "match_found"},
    {MessageType::NoMatchFound, "no_match_found"},
    {MessageType::Spectating, "spectating"},
    {MessageType::RoomCreateFail, "room_create_fail"},
    {MessageType::Snapshot, "snapshot"},
    {MessageType::OpponentDisconnected, "opponent_disconnected"},
    {MessageType::GameOver, "game_over"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LoginMsg, username, password)

// Empty struct: NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE requires at least one
// field, so these two are hand-written instead.
inline void to_json(nlohmann::json& j, const PlayMsg&) { j = nlohmann::json::object(); }
inline void from_json(const nlohmann::json&, PlayMsg&) {}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoomJoinMsg, roomName)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoomCreateMsg, roomName)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClickMsg, row, col)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JumpMsg, row, col)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LoginOkMsg, elo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LoginFailMsg, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MatchFoundMsg, opponentUsername, assignedColor)

inline void to_json(nlohmann::json& j, const NoMatchFoundMsg&) { j = nlohmann::json::object(); }
inline void from_json(const nlohmann::json&, NoMatchFoundMsg&) {}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SpectatingMsg, roomName)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RoomCreateFailMsg, reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PieceSnapshot, id, color, kind, cell, state, hasMoved, activeMove, activeRest)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SnapshotMsg, pieces, activeJump, elapsedMs, score, gameOver, moveHistoryDelta)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OpponentDisconnectedMsg, secondsLeft)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameOverMsg, winner, reason)

}  // namespace net

#pragma once


#include <optional>
#include <string>
#include <vector>

#include "model/GameOverState.hpp"
#include "model/MoveRecord.hpp"
#include "model/Piece.hpp"
#include "model/PieceJump.hpp"
#include "model/PieceMove.hpp"
#include "model/PieceRest.hpp"
#include "model/Position.hpp"
#include "model/Score.hpp"

namespace net {


enum class MessageType {
    // Client -> Server
    Login,
    Play,
    RoomJoin,
    RoomCreate,
    Click,
    Jump,

    // Server -> Client
    LoginOk,
    LoginFail,
    MatchFound,
    NoMatchFound,
    Spectating,
    RoomCreateFail,
    Snapshot,
    OpponentDisconnected,
    GameOver,
};

// ---------------------------------------------------------------------------
// Client -> Server
// ---------------------------------------------------------------------------

struct LoginMsg {
    std::string username;
    std::string password;
};

struct PlayMsg {};

struct RoomJoinMsg {
    std::string roomName;
};

// Unlike RoomJoinMsg, this fails (RoomCreateFailMsg) if roomName is already
// taken - it only ever creates a brand-new room, never joins an existing one.
struct RoomCreateMsg {
    std::string roomName;
};

struct ClickMsg {
    int row;
    int col;
};

struct JumpMsg {
    int row;
    int col;
};

// ---------------------------------------------------------------------------
// Server -> Client
// ---------------------------------------------------------------------------

struct LoginOkMsg {
    int elo;
};

struct LoginFailMsg {
    std::string reason;
};

struct MatchFoundMsg {
    std::string opponentUsername;
    Color       assignedColor;  
};

struct NoMatchFoundMsg {};

struct SpectatingMsg {
    std::string roomName;
};

struct RoomCreateFailMsg {
    std::string reason;  // e.g. "room already exists"
};


struct PieceSnapshot {
    int                       id;
    Color                     color;
    Kind                      kind;
    Position                  cell;
    PieceState                state;
    bool                      hasMoved;
    std::optional<PieceMove>  activeMove;  // from arbiter.activeMoveFor(cell)
    std::optional<PieceRest>  activeRest;  // from arbiter.activeRest(id)
};

struct SnapshotMsg {
    std::vector<PieceSnapshot> pieces;
    std::optional<PieceJump> activeJump;

    long elapsedMs;

    Score         score;
    GameOverState gameOver;

    std::vector<MoveRecord> moveHistoryDelta;
};

struct OpponentDisconnectedMsg {
    int secondsLeft;
};

struct GameOverMsg {
    Color          winner;
    GameOverReason reason;
};

}  // namespace net

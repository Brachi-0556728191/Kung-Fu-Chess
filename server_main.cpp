#include "GameInstance.hpp"
#include "messages_envelope.h"
#include "io/BoardParser.hpp"
#include "input/Controller.hpp"
#include "engine/GameEngine.hpp"
#include "rules/config.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kPort = 9010;

Board standardStartingBoard() {
    return parseBoard({
        "bR bN bB bQ bK bB bN bR",
        "bP bP bP bP bP bP bP bP",
        ". . . . . . . .",
        ". . . . . . . .",
        ". . . . . . . .",
        ". . . . . . . .",
        "wP wP wP wP wP wP wP wP",
        "wR wN wB wQ wK wB wN wR"
    });
}

struct ConnectionInfo {
    Color  color;
    size_t moveHistorySent = 0;
};

std::mutex connectionsMutex;
std::unordered_map<ix::WebSocket*, ConnectionInfo> connections;
int nextColorSlot = 0;  // guarded by connectionsMutex; 0 -> White, 1 -> Black, 2+ unassigned

bool isClickAllowed(const GameState& state, Position cell, Color connectionColor) {
    const Selection& mine = state.selections.forColor(connectionColor);
    if (mine.active) {
        std::optional<Piece> selectedPiece = state.board.pieceAt(mine.cell);
        return selectedPiece && selectedPiece->color == connectionColor;
    }
    std::optional<Piece> clickedPiece = state.board.pieceAt(cell);
    return !clickedPiece || clickedPiece->color == connectionColor;
}

bool isJumpAllowed(const GameState& state, Position cell, Color connectionColor) {
    std::optional<Piece> piece = state.board.pieceAt(cell);
    return piece && piece->color == connectionColor;
}

// Pieces/activeJump/elapsedMs/score/gameOver only - moveHistoryDelta is left
// empty here and filled in per-connection by the caller, since each
// connection has its own "how much moveHistory have I already sent" watermark.
net::SnapshotMsg buildBaseSnapshot(const GameState& state) {
    net::SnapshotMsg snap;
    for (int r = 0; r < state.board.rows(); ++r) {
        for (int c = 0; c < state.board.cols(); ++c) {
            std::optional<Piece> piece = state.board.pieceAt(Position{r, c});
            if (!piece) continue;

            net::PieceSnapshot ps;
            ps.id         = piece->id;
            ps.color      = piece->color;
            ps.kind       = piece->kind;
            ps.cell       = piece->cell;
            ps.state      = piece->state;
            ps.hasMoved   = piece->hasMoved;
            ps.activeMove = state.arbiter.activeMoveFor(piece->cell);
            ps.activeRest = state.arbiter.activeRest(piece->id);
            snap.pieces.push_back(ps);
        }
    }
    snap.activeJump = state.arbiter.activeJump();
    snap.elapsedMs  = state.elapsedMs;
    snap.score      = state.score;
    snap.gameOver   = state.gameOver;
    return snap;
}

int canonicalX(int col) {
    return config::HISTORY_PANEL_WIDTH + col * config::CELL_SIZE + config::CELL_SIZE / 2;
}

int canonicalY(int row) {
    return row * config::CELL_SIZE + config::CELL_SIZE / 2;
}

}  // namespace

int main() {
    ix::initNetSystem();

    GameInstance instance;
    instance.state.board = standardStartingBoard();

    ix::WebSocketServer server(kPort, "0.0.0.0");

    server.setOnClientMessageCallback(
        [&instance](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& webSocket,
                    const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                std::lock_guard<std::mutex> lock(connectionsMutex);
                if (nextColorSlot < 2) {
                    Color color = (nextColorSlot == 0) ? Color::White : Color::Black;
                    connections[&webSocket] = ConnectionInfo{color, 0};
                    ++nextColorSlot;
                    std::cout << "Client connected, assigned "
                              << (color == Color::White ? "White" : "Black") << std::endl;
                } else {
                    std::cout << "Client connected, no color slot available" << std::endl;
                }
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                std::lock_guard<std::mutex> lock(connectionsMutex);
                connections.erase(&webSocket);
                std::cout << "Client disconnected" << std::endl;
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                std::optional<Color> color;
                {
                    std::lock_guard<std::mutex> lock(connectionsMutex);
                    auto it = connections.find(&webSocket);
                    if (it != connections.end()) color = it->second.color;
                }
                if (!color) return;  // unassigned connection (3rd+): ignored for S4

                nlohmann::json envelope;
                net::MessageType type;
                try {
                    envelope = nlohmann::json::parse(msg->str);
                    type = net::peekType(envelope);
                } catch (const std::exception&) {
                    return;  // malformed message: ignored
                }

                if (type == net::MessageType::Click) {
                    net::ClickMsg click = envelope.get<net::ClickMsg>();
                    Position cell{click.row, click.col};

                    std::lock_guard<std::mutex> lock(instance.mutex);
                    if (isClickAllowed(instance.state, cell, *color)) {
                        // Controller::click's signature is protected and can't take a
                        // Color, so state.selection is used as a transient scratch slot:
                        // load this connection's real selection in, let Controller::click
                        // run completely unmodified, then save it back out. See PLAN.md's
                        // S4.5 entry.
                        Selection& mine = instance.state.selections.forColor(*color);
                        std::swap(instance.state.selection, mine);
                        Controller::click(instance.state, canonicalX(click.col), canonicalY(click.row));
                        std::swap(instance.state.selection, mine);
                    }
                } else if (type == net::MessageType::Jump) {
                    net::JumpMsg jump = envelope.get<net::JumpMsg>();
                    Position cell{jump.row, jump.col};

                    std::lock_guard<std::mutex> lock(instance.mutex);
                    if (isJumpAllowed(instance.state, cell, *color)) {
                        Controller::jump(instance.state, canonicalX(jump.col), canonicalY(jump.row));
                    }
                }
                // Other message types (login/play/room_*) aren't implemented until later stages.
            }
        });

    auto result = server.listen();
    if (!result.first) {
        std::cerr << "Failed to listen on port " << kPort << ": " << result.second << std::endl;
        ix::uninitNetSystem();
        return 1;
    }

    server.start();
    std::cout << "Server listening on port " << kPort << std::endl;

    auto lastTick = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count());
        lastTick = now;

        {
            std::lock_guard<std::mutex> lock(instance.mutex);
            handleWait(instance.state, elapsedMs);
        }

        net::SnapshotMsg         baseSnapshot;
        std::vector<MoveRecord>  fullHistory;
        {
            std::lock_guard<std::mutex> lock(instance.mutex);
            baseSnapshot = buildBaseSnapshot(instance.state);
            fullHistory  = instance.state.moveHistory;
        }

        for (const auto& client : server.getClients()) {
            size_t alreadySent;
            bool   found = false;
            {
                std::lock_guard<std::mutex> lock(connectionsMutex);
                auto it = connections.find(client.get());
                if (it != connections.end()) {
                    alreadySent = it->second.moveHistorySent;
                    found = true;
                }
            }
            if (!found) continue;

            net::SnapshotMsg snap = baseSnapshot;
            if (alreadySent < fullHistory.size()) {
                snap.moveHistoryDelta.assign(fullHistory.begin() + alreadySent, fullHistory.end());
            }

            nlohmann::json envelope = net::toEnvelope(net::MessageType::Snapshot, snap);
            client->send(envelope.dump());

            std::lock_guard<std::mutex> lock(connectionsMutex);
            auto it = connections.find(client.get());
            if (it != connections.end()) it->second.moveHistorySent = fullHistory.size();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    ix::uninitNetSystem();
    return 0;
}

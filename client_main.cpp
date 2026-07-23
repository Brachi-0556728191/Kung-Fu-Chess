#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "messages_envelope.h"
#include "io/BoardParser.hpp"
#include "input/BoardMapper.hpp"
#include "view/renderer.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <iostream>
#include <mutex>
#include <optional>

#include <opencv2/highgui.hpp>

namespace {

constexpr const char* kServerUrl = "ws://localhost:9010";

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

struct ClientContext {
    GameState      state;
    std::mutex     stateMutex;
    ix::WebSocket* webSocket = nullptr;  // set once, after construction, in main()
};

void applySnapshot(GameState& state, const net::SnapshotMsg& snap) {
    Board newBoard(state.board.rows(), state.board.cols());
    for (const net::PieceSnapshot& ps : snap.pieces) {
        Piece piece;
        piece.id       = ps.id;
        piece.color    = ps.color;
        piece.kind     = ps.kind;
        piece.cell     = ps.cell;
        piece.state    = ps.state;
        piece.hasMoved = ps.hasMoved;
        newBoard.addPiece(piece);
    }
    state.board = std::move(newBoard);

    state.elapsedMs = snap.elapsedMs;
    state.score     = snap.score;
    state.gameOver  = snap.gameOver;
    state.moveHistory.insert(state.moveHistory.end(),
                              snap.moveHistoryDelta.begin(), snap.moveHistoryDelta.end());
   
}

void sendMessage(ClientContext& ctx, net::MessageType type, const nlohmann::json& payload) {
    if (!ctx.webSocket) return;
    nlohmann::json envelope = payload;
    envelope["type"] = type;
    ctx.webSocket->send(envelope.dump());
}


void onMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    ClientContext& ctx = *static_cast<ClientContext*>(userdata);
    if (event != cv::EVENT_LBUTTONDOWN && event != cv::EVENT_LBUTTONDBLCLK) return;

    std::lock_guard<std::mutex> lock(ctx.stateMutex);

    auto cell = pixelToCell(x, y, ctx.state.board);
    if (!cell) {
        ctx.state.selection = Selection{};
        return;
    }

    if (event == cv::EVENT_LBUTTONDBLCLK) {
        sendMessage(ctx, net::MessageType::Jump, net::JumpMsg{cell->row, cell->col});
        return;
    }

    std::optional<Piece> clicked = ctx.state.board.pieceAt(*cell);
    if (ctx.state.selection.active) {
        std::optional<Piece> selected = ctx.state.board.pieceAt(ctx.state.selection.cell);
        bool sameSide = clicked && selected && clicked->color == selected->color;
        ctx.state.selection = sameSide ? Selection{true, *cell, ctx.state.elapsedMs} : Selection{};
    } else if (clicked) {
        ctx.state.selection = {true, *cell, ctx.state.elapsedMs};
    }

    sendMessage(ctx, net::MessageType::Click, net::ClickMsg{cell->row, cell->col});
}

}  // namespace

int main() {
#ifdef _WIN32
    SetProcessDPIAware();
#endif

    ix::initNetSystem();

    ClientContext ctx;
    ctx.state.board = standardStartingBoard();

    ix::WebSocket webSocket;
    webSocket.setUrl(kServerUrl);
    ctx.webSocket = &webSocket;

    webSocket.setOnMessageCallback([&ctx](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            nlohmann::json   envelope;
            net::MessageType type;
            try {
                envelope = nlohmann::json::parse(msg->str);
                type = net::peekType(envelope);
            } catch (const std::exception&) {
                return;  // malformed message: ignored
            }
            if (type != net::MessageType::Snapshot) return;

            net::SnapshotMsg snap = envelope.get<net::SnapshotMsg>();

            std::lock_guard<std::mutex> lock(ctx.stateMutex);
            applySnapshot(ctx.state, snap);
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "Connected to server" << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
        }
    });

    webSocket.start();

    const std::string windowName = "Chess (client)";
    cv::namedWindow(windowName);
    cv::setMouseCallback(windowName, onMouse, &ctx);

    while (true) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(ctx.stateMutex);
            frame = view::renderBoard(ctx.state);
        }
        cv::imshow(windowName, frame);

        int key = cv::waitKey(16);
        if (key == 27 || cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) < 1) break;
    }

    webSocket.stop();
    ix::uninitNetSystem();
    return 0;
}

// S1 - proves the raw WebSocket pipe works before anything is built on top of
// it. Deliberately standalone: no dependency on any existing game code.
// S3 - now decodes the incoming text as a real net::ClickMsg and re-encodes
// it before echoing, instead of echoing the raw string back untouched. Still
// zero game logic: this only proves struct -> JSON -> struct works over the
// real socket, same scope as S1 otherwise.
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <iostream>

#include "messages_json.h"

namespace {
constexpr int kPort = 9002;
}

int main() {
    ix::initNetSystem();

    ix::WebSocketServer server(kPort, "0.0.0.0");

    server.setOnClientMessageCallback(
        [](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& webSocket,
           const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message) {
                std::cout << "Server received: " << msg->str << std::endl;

                net::ClickMsg click = nlohmann::json::parse(msg->str).get<net::ClickMsg>();
                std::cout << "Decoded ClickMsg{row=" << click.row << ", col=" << click.col << "}"
                          << std::endl;

                nlohmann::json reply = click;
                webSocket.send(reply.dump());
            } else if (msg->type == ix::WebSocketMessageType::Open) {
                std::cout << "Client connected" << std::endl;
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                std::cout << "Client disconnected" << std::endl;
            }
        });

    auto result = server.listen();
    if (!result.first) {
        std::cerr << "Failed to listen on port " << kPort << ": " << result.second << std::endl;
        ix::uninitNetSystem();
        return 1;
    }

    server.start();
    std::cout << "Echo server listening on port " << kPort << std::endl;

    server.wait();  // blocks until server.stop() is called

    ix::uninitNetSystem();
    return 0;
}

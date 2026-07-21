// S1 - the client half of the echo round-trip proof. Standalone, no
// dependency on any existing game code.
// S3 - now sends a real net::ClickMsg serialized to JSON instead of the
// literal string "ping", and decodes the echoed reply back into a struct.
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "messages_json.h"

int main() {
    ix::initNetSystem();

    ix::WebSocket webSocket;
    webSocket.setUrl("ws://localhost:9002");

    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::cout << "Client received: " << msg->str << std::endl;
            net::ClickMsg reply = nlohmann::json::parse(msg->str).get<net::ClickMsg>();
            std::cout << "Decoded ClickMsg{row=" << reply.row << ", col=" << reply.col << "}"
                       << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "Connected to server" << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
        }
    });

    webSocket.start();

    // Give the handshake a moment to complete before sending.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    net::ClickMsg click{3, 4};
    nlohmann::json j = click;
    std::cout << "Sending: " << j.dump() << std::endl;
    webSocket.send(j.dump());

    // Long enough for the echoed reply to arrive and print via the callback above.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    webSocket.stop();
    ix::uninitNetSystem();
    return 0;
}

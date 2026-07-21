// S1 - the client half of the echo round-trip proof. Standalone, no
// dependency on any existing game code.
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    ix::initNetSystem();

    ix::WebSocket webSocket;
    webSocket.setUrl("ws://localhost:9002");

    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::cout << "Client received: " << msg->str << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "Connected to server" << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            std::cout << "Connection error: " << msg->errorInfo.reason << std::endl;
        }
    });

    webSocket.start();

    // Give the handshake a moment to complete before sending.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Sending: ping" << std::endl;
    webSocket.send("ping");

    // Long enough for the echoed reply to arrive and print via the callback above.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    webSocket.stop();
    ix::uninitNetSystem();
    return 0;
}

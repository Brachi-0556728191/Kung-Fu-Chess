#include "model/Board.hpp"
#include "model/GameState.hpp"
#include "io/BoardParser.hpp"
#include "input/Controller.hpp"
#include "engine/GameEngine.hpp"
#include "view/renderer.hpp"

#include <chrono>

#include <opencv2/highgui.hpp>

namespace {

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

void onMouse(int event, int x, int y, int /*flags*/, void* userdata) {
    GameState& state = *static_cast<GameState*>(userdata);
    if (event == cv::EVENT_LBUTTONDOWN) {
        Controller::click(state, x, y);
    } else if (event == cv::EVENT_LBUTTONDBLCLK) {
        Controller::jump(state, x, y);
    }
}

}  // namespace

int main() {
    GameState state;
    state.board = standardStartingBoard();

    const std::string windowName = "Chess";
    cv::namedWindow(windowName);
    cv::setMouseCallback(windowName, onMouse, &state);

    auto lastTick = std::chrono::steady_clock::now();

    while (true) {
        cv::imshow(windowName, view::renderBoard(state));

        int key = cv::waitKey(16);
        if (key == 27 || cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) < 1) break;

        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = static_cast<long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count());
        lastTick = now;
        handleWait(state, elapsedMs);
    }

    return 0;
}
// https://github.com/Brachi-0556728191/Kung-Fu-Chess/tree/main

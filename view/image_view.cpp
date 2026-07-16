#include "image_view.hpp"
#include "renderer.hpp"
#include "img.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "../rules/config.hpp"

#ifndef CHESS_ASSETS_DIR
#define CHESS_ASSETS_DIR "assets"
#endif

namespace view {

namespace {

const cv::Scalar LIGHT_SQUARE(181, 217, 240);
const cv::Scalar DARK_SQUARE(99, 136, 181);
const cv::Scalar SELECTION_BORDER(60, 220, 60);

// The sprite PNGs have no alpha channel - they're flat-color background.
// Derive transparency from how close each pixel is to the background color
// (sampled from the top-left corner), with a soft ramp so edges anti-alias
// instead of coming out jagged.
cv::Mat addAlphaFromBackground(const cv::Mat& bgr) {
    const cv::Vec3b bg = bgr.at<cv::Vec3b>(0, 0);
    constexpr int LOW_DIST = 15;
    constexpr int HIGH_DIST = 60;

    cv::Mat alpha(bgr.size(), CV_8UC1);
    for (int y = 0; y < bgr.rows; ++y) {
        for (int x = 0; x < bgr.cols; ++x) {
            const cv::Vec3b& px = bgr.at<cv::Vec3b>(y, x);
            int dist = std::abs(px[0] - bg[0]) + std::abs(px[1] - bg[1]) + std::abs(px[2] - bg[2]);
            int a = ((dist - LOW_DIST) * 255) / (HIGH_DIST - LOW_DIST);
            alpha.at<uchar>(y, x) = static_cast<uchar>(std::clamp(a, 0, 255));
        }
    }

    std::vector<cv::Mat> bgrChannels;
    cv::split(bgr, bgrChannels);
    bgrChannels.push_back(alpha);

    cv::Mat bgra;
    cv::merge(bgrChannels, bgra);
    return bgra;
}

void overlayImage(cv::Mat& background, const cv::Mat& foreground, cv::Point location) {
    for (int y = 0; y < foreground.rows; ++y) {
        int bgY = location.y + y;
        if (bgY < 0 || bgY >= background.rows) continue;
        for (int x = 0; x < foreground.cols; ++x) {
            int bgX = location.x + x;
            if (bgX < 0 || bgX >= background.cols) continue;

            cv::Vec3b& bg = background.at<cv::Vec3b>(bgY, bgX);
            if (foreground.channels() == 4) {
                const cv::Vec4b& fg = foreground.at<cv::Vec4b>(y, x);
                double alpha = fg[3] / 255.0;
                for (int c = 0; c < 3; ++c)
                    bg[c] = static_cast<uchar>(fg[c] * alpha + bg[c] * (1.0 - alpha));
            } else {
                bg = foreground.at<cv::Vec3b>(y, x);
            }
        }
    }
}

std::string pieceFolder(Kind kind, Color color) {
    std::string folder(1, charFromKind(kind));
    folder += (color == Color::White) ? 'W' : 'B';
    return folder;
}

std::string stateDir(Kind kind, Color color, AnimationState state) {
    return std::string(CHESS_ASSETS_DIR) + "/Pieces/" + pieceFolder(kind, color) +
           "/states/" + animationStateFolder(state);
}

}  // namespace

std::string pieceSpritePath(Kind kind, Color color, AnimationState state, int frameIndex) {
    return stateDir(kind, color, state) + "/sprites/" + std::to_string(frameIndex + 1) + ".png";
}

cv::Mat loadPieceSprite(Kind kind, Color color, AnimationState state, int frameIndex) {
    static std::map<std::string, cv::Mat> cache;

    std::string path = pieceSpritePath(kind, color, state, frameIndex);
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    // Required course library handles loading + resizing; it throws
    // std::runtime_error itself if the file can't be read.
    Img loader;
    loader.read(path, {config::CELL_SIZE, config::CELL_SIZE}, /*keep_aspect=*/false, cv::INTER_LINEAR);
    cv::Mat sprite = loader.get_mat();

    // The sprites have no alpha channel, and Img doesn't synthesize one -
    // this stays our own code regardless of the loading backend.
    if (sprite.channels() == 3) sprite = addAlphaFromBackground(sprite);

    cache[path] = sprite;
    return sprite;
}

AnimationTiming loadAnimationTiming(Kind kind, Color color, AnimationState state) {
    static std::map<std::string, AnimationTiming> cache;

    std::string path = stateDir(kind, color, state) + "/config.json";
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open animation config: " + path);
    std::ostringstream buf;
    buf << file.rdbuf();
    std::string text = buf.str();

    // Deliberately not a general JSON parser - the config shape is small and
    // fixed (two leaf fields we care about), so pulling in a JSON library
    // just for this would be a bigger dependency than the problem warrants.
    AnimationTiming timing{6.0, true};
    std::smatch match;
    if (std::regex_search(text, match, std::regex(R"("frames_per_sec"\s*:\s*([0-9.]+))")))
        timing.framesPerSec = std::stod(match[1]);
    if (std::regex_search(text, match, std::regex(R"("is_loop"\s*:\s*(true|false))")))
        timing.isLoop = (match[1] == "true");

    cache[path] = timing;
    return timing;
}

cv::Mat renderBoard(const GameState& state) {
    const Board& board = state.board;
    int rows = board.rows();
    int cols = board.cols();
    cv::Mat image(rows * config::CELL_SIZE, cols * config::CELL_SIZE, CV_8UC3, cv::Scalar(0, 0, 0));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            cv::Rect cell(c * config::CELL_SIZE, r * config::CELL_SIZE, config::CELL_SIZE, config::CELL_SIZE);
            image(cell) = ((r + c) % 2 == 0) ? LIGHT_SQUARE : DARK_SQUARE;
        }
    }

    if (state.selection.active) {
        cv::Rect cell(state.selection.cell.col * config::CELL_SIZE, state.selection.cell.row * config::CELL_SIZE,
                      config::CELL_SIZE, config::CELL_SIZE);
        cv::rectangle(image, cell, SELECTION_BORDER, 4);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto piece = board.pieceAt(Position{r, c});
            if (!piece) continue;

            AnimationInfo anim = resolveAnimationState(*piece, state.arbiter);
            AnimationTiming timing = loadAnimationTiming(piece->kind, piece->color, anim.state);
            int frame = animationFrameIndex(state.elapsedMs, anim, timing);
            cv::Mat sprite = loadPieceSprite(piece->kind, piece->color, anim.state, frame);

            // A piece still sits at its board cell (Position{r, c}) in the
            // Board data for the whole flight - RealTimeArbiter only moves
            // it once the timed motion lands. So for every state except
            // Move, draw at the literal cell; for Move, interpolate between
            // from/to using the same PieceMove the arbiter is tracking.
            cv::Point drawAt(c * config::CELL_SIZE, r * config::CELL_SIZE);
            if (anim.state == AnimationState::Move) {
                // guaranteed present: anim.state == Move implies resolveAnimationState
                // found a move whose `from` is this exact cell.
                PieceMove move = *state.arbiter.activeMoveFor(Position{r, c});
                double t = (move.durationMs > 0)
                               ? double(state.elapsedMs - move.startMs) / double(move.durationMs)
                               : 1.0;
                t = std::clamp(t, 0.0, 1.0);

                double row = move.from.row + (move.to.row - move.from.row) * t;
                double col = move.from.col + (move.to.col - move.from.col) * t;
                drawAt = cv::Point(static_cast<int>(col * config::CELL_SIZE),
                                    static_cast<int>(row * config::CELL_SIZE));
            }

            overlayImage(image, sprite, drawAt);
        }
    }

    return image;
}

void showBoard(const GameState& state, const std::string& windowName) {
    cv::Mat image = renderBoard(state);
    cv::imshow(windowName, image);
    cv::waitKey(0);
    cv::destroyWindow(windowName);
}

}  // namespace view

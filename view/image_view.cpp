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

#include "../model/GameOverState.hpp"
#include "../model/MoveRecord.hpp"
#include "../rules/RuleEngine.hpp"
#include "../rules/config.hpp"

#ifndef CHESS_ASSETS_DIR
#define CHESS_ASSETS_DIR "assets"
#endif

namespace view {

namespace {

const cv::Scalar SELECTION_BORDER(60, 220, 60);

const cv::Scalar REST_OVERLAY_COLOR(245, 250, 250);
constexpr double REST_OVERLAY_MAX_ALPHA = 0.55;

// Light, semi-transparent tint for a selected piece's legal destination
// squares - same cv::addWeighted technique as REST_OVERLAY, just covering
// the whole cell instead of a draining partial height.
const cv::Scalar LEGAL_MOVE_HIGHLIGHT_COLOR(140, 245, 140);
constexpr double LEGAL_MOVE_HIGHLIGHT_ALPHA = 0.35;


const cv::Scalar HISTORY_BG_COLOR(40, 40, 40);
const cv::Scalar HISTORY_TEXT_COLOR(225, 225, 225);
const cv::Scalar HISTORY_HEADER_COLOR(0, 215, 255);
const cv::Scalar HISTORY_SCORE_COLOR(120, 255, 120);

constexpr int HISTORY_HEADER_Y = 30;
constexpr int HISTORY_SCORE_Y = 54;
constexpr int HISTORY_FIRST_ROW_Y = 84;
constexpr int HISTORY_ROW_HEIGHT = 24;
constexpr double HISTORY_ROW_FONT_SCALE = 0.5;

const cv::Scalar COORDINATE_LABEL_COLOR(40, 40, 40);
constexpr double COORDINATE_LABEL_FONT_SCALE = 0.4;

// Whole-canvas dim + centered title/subtitle for the game-over overlay.
// Same cv::addWeighted dim-then-draw technique used throughout this file,
// just applied to the entire frame instead of a single cell.
const cv::Scalar GAME_OVER_DIM_COLOR(0, 0, 0);
constexpr double GAME_OVER_DIM_ALPHA = 0.55;
const cv::Scalar GAME_OVER_TITLE_COLOR(255, 255, 255);
const cv::Scalar GAME_OVER_SUBTITLE_COLOR(0, 215, 255);


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

// Draws the cooling time
void drawRestOverlay(cv::Mat& image, int boardX, int row, int col, double remainingFraction) {
    int overlayHeight = static_cast<int>(config::CELL_SIZE * remainingFraction);
    if (overlayHeight <= 0) return;

    cv::Rect overlayRect(boardX + col * config::CELL_SIZE,
                          row * config::CELL_SIZE + (config::CELL_SIZE - overlayHeight),
                          config::CELL_SIZE, overlayHeight);

    cv::Mat roi = image(overlayRect);
    cv::Mat tint(roi.size(), roi.type(), REST_OVERLAY_COLOR);
    cv::addWeighted(tint, REST_OVERLAY_MAX_ALPHA, roi, 1.0 - REST_OVERLAY_MAX_ALPHA, 0.0, roi);
}

// Tints one full cell to mark it as a legal destination for the currently
void drawLegalMoveHighlight(cv::Mat& image, int boardX, Position pos) {
    cv::Rect cellRect(boardX + pos.col * config::CELL_SIZE, pos.row * config::CELL_SIZE,
                       config::CELL_SIZE, config::CELL_SIZE);
    cv::Mat roi = image(cellRect);
    cv::Mat tint(roi.size(), roi.type(), LEGAL_MOVE_HIGHLIGHT_COLOR);
    cv::addWeighted(tint, LEGAL_MOVE_HIGHLIGHT_ALPHA, roi, 1.0 - LEGAL_MOVE_HIGHLIGHT_ALPHA, 0.0, roi);
}

// return match letters
char fileLetter(int col) {
    return static_cast<char>('a' + col);
}

// return match numbers
int rankNumber(int row, int boardRows) {
    return boardRows - row;
}

// e.g. Position{3,0} on an 8-row board -> "a5".
std::string squareLabel(Position pos, int boardRows) {
    return std::string(1, fileLetter(pos.col)) + std::to_string(rankNumber(pos.row, boardRows));
}


std::string moveHistoryLine(int moveNumber, const MoveRecord& rec, int boardRows) {
    std::ostringstream out;
    out << moveNumber << ". " << charFromKind(rec.kind)
        << " (" << squareLabel(rec.from, boardRows) << ")"
        << "->(" << squareLabel(rec.to, boardRows) << ")";
    if (rec.captured) out << " x";
    return out.str();
}


void drawHistoryPanel(cv::Mat& image, int panelX, int panelWidth, int panelHeight,
                       const std::string& title, int score,
                       const std::vector<MoveRecord>& moves, int boardRows) {
    cv::Rect panelRect(panelX, 0, panelWidth, panelHeight);
    image(panelRect) = HISTORY_BG_COLOR;

    cv::putText(image, title, cv::Point(panelX + 10, HISTORY_HEADER_Y),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, HISTORY_HEADER_COLOR, 2, cv::LINE_AA);

    cv::putText(image, "Score: " + std::to_string(score), cv::Point(panelX + 10, HISTORY_SCORE_Y),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, HISTORY_SCORE_COLOR, 1, cv::LINE_AA);

    int maxRows = std::max(0, (panelHeight - HISTORY_FIRST_ROW_Y) / HISTORY_ROW_HEIGHT);
    int total = static_cast<int>(moves.size());
    int start = std::max(0, total - maxRows);

    int y = HISTORY_FIRST_ROW_Y;
    for (int i = start; i < total; ++i) {
        cv::putText(image, moveHistoryLine(i + 1, moves[i], boardRows), cv::Point(panelX + 10, y),
                    cv::FONT_HERSHEY_SIMPLEX, HISTORY_ROW_FONT_SCALE, HISTORY_TEXT_COLOR, 1, cv::LINE_AA);
        y += HISTORY_ROW_HEIGHT;
    }
}

// draw Rank Label on left colum.
void drawRankLabel(cv::Mat& image, int boardX, int row, int boardRows) {
    cv::putText(image, std::to_string(rankNumber(row, boardRows)),
                cv::Point(boardX + 4, row * config::CELL_SIZE + 16),
                cv::FONT_HERSHEY_SIMPLEX, COORDINATE_LABEL_FONT_SCALE, COORDINATE_LABEL_COLOR, 1, cv::LINE_AA);
}

// draw Rank Label on botton row.
void drawFileLabel(cv::Mat& image, int boardX, int col, int boardRows) {
    int row = boardRows - 1;
    cv::putText(image, std::string(1, fileLetter(col)),
                cv::Point(boardX + col * config::CELL_SIZE + config::CELL_SIZE - 14,
                          row * config::CELL_SIZE + config::CELL_SIZE - 6),
                cv::FONT_HERSHEY_SIMPLEX, COORDINATE_LABEL_FONT_SCALE, COORDINATE_LABEL_COLOR, 1, cv::LINE_AA);
}


void drawGameOverOverlay(cv::Mat& image, const GameOverState& gameOver) {
    cv::Mat dim(image.size(), image.type(), GAME_OVER_DIM_COLOR);
    cv::addWeighted(dim, GAME_OVER_DIM_ALPHA, image, 1.0 - GAME_OVER_DIM_ALPHA, 0.0, image);

    const std::string title = "GAME OVER";
    const std::string subtitle = (gameOver.winner == Color::White ? "White" : "Black") + std::string(" wins!");

    int baseline = 0;
    const double titleScale = 1.4;
    cv::Size titleSize = cv::getTextSize(title, cv::FONT_HERSHEY_SIMPLEX, titleScale, 3, &baseline);
    cv::Point titlePos((image.cols - titleSize.width) / 2, image.rows / 2 - 10);
    cv::putText(image, title, titlePos, cv::FONT_HERSHEY_SIMPLEX, titleScale, GAME_OVER_TITLE_COLOR, 3, cv::LINE_AA);

    const double subScale = 0.8;
    cv::Size subSize = cv::getTextSize(subtitle, cv::FONT_HERSHEY_SIMPLEX, subScale, 2, &baseline);
    cv::Point subPos((image.cols - subSize.width) / 2, image.rows / 2 + 30);
    cv::putText(image, subtitle, subPos, cv::FONT_HERSHEY_SIMPLEX, subScale, GAME_OVER_SUBTITLE_COLOR, 2, cv::LINE_AA);
}

cv::Mat loadBoardBackground(int width, int height) {
    static std::map<std::string, cv::Mat> cache;

    std::string key = std::to_string(width) + "x" + std::to_string(height);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    Img loader;
    loader.read(std::string(CHESS_ASSETS_DIR) + "/board.png", {width, height},
                /*keep_aspect=*/false, cv::INTER_LINEAR);
    cv::Mat background = loader.get_mat();
    if (background.channels() == 4) cv::cvtColor(background, background, cv::COLOR_BGRA2BGR);

    cache[key] = background;
    return background;
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

}  

std::string pieceSpritePath(Kind kind, Color color, AnimationState state, int frameIndex) {
    return stateDir(kind, color, state) + "/sprites/" + std::to_string(frameIndex + 1) + ".png";
}

cv::Mat loadPieceSprite(Kind kind, Color color, AnimationState state, int frameIndex) {
    static std::map<std::string, cv::Mat> cache;

    std::string path = pieceSpritePath(kind, color, state, frameIndex);
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    Img loader;
    loader.read(path, {config::CELL_SIZE, config::CELL_SIZE}, /*keep_aspect=*/false, cv::INTER_LINEAR);
    cv::Mat sprite = loader.get_mat();

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
    int boardWidth  = cols * config::CELL_SIZE;
    int boardHeight = rows * config::CELL_SIZE;

    int boardX = config::HISTORY_PANEL_WIDTH;
    int canvasWidth = config::HISTORY_PANEL_WIDTH + boardWidth + config::HISTORY_PANEL_WIDTH;

    // יצירת תמונה ריקה בגודל המתאים ללוח (שורות כפול גודל תא) עם 3 ערוצי צבע (RGB) וצבע רקע שחור.
    cv::Mat image(boardHeight, canvasWidth, CV_8UC3, cv::Scalar(0, 0, 0));

    // הדבקת תמונת הרקע של הלוח 
    cv::Mat boardBackground = loadBoardBackground(boardWidth, boardHeight);
    boardBackground.copyTo(image(cv::Rect(boardX, 0, boardWidth, boardHeight)));

    // בדיקה האם יש כלי שנבחר על ידי השחקן.
    if (state.selection.active) {
        cv::Rect cell(boardX + state.selection.cell.col * config::CELL_SIZE, state.selection.cell.row * config::CELL_SIZE,
                        //חישוב המיקום בפיקסלים של התא שנבחר
                      config::CELL_SIZE, config::CELL_SIZE);
        // ציור מסגרת מסביב לתא שנבחר בעובי 4 פיקסלים.
        cv::rectangle(image, cell, SELECTION_BORDER, 4);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto piece = board.pieceAt(Position{r, c});
            if (!piece) continue;

            // Resolve this piece's move/jump/rest data once, up front - these
            // three queries are the exact raw data both resolveAnimationState
            // and restRemainingFraction need (see AnimationState.hpp), and a
            // future network snapshot would carry precisely this same data
            // instead of a live arbiter reference.
            auto activeMove = state.arbiter.activeMoveFor(Position{r, c});
            auto activeJump = state.arbiter.activeJump();
            auto activeRest = state.arbiter.activeRest(piece->id);

            /// ביעת מצב האנימציה הנוכחי של הכלי (תזוזה, קפיצה, מנוחה וכו').
            AnimationInfo anim = resolveAnimationState(*piece, activeMove, activeJump, activeRest);
            // טעינת נתוני תזמון (קצב פריימים ולולאה) לפי סוג ומצב הכלי.
            AnimationTiming timing = loadAnimationTiming(piece->kind, piece->color, anim.state);
            int frame = animationFrameIndex(state.elapsedMs, anim, timing);
            // טעינת תמונת הפריים המתאימה לכלי.
            cv::Mat sprite = loadPieceSprite(piece->kind, piece->color, anim.state, frame);


            // קביעת מיקום ברירת המחדל לציור הכלי בפיקסלים.
            cv::Point drawAt(boardX + c * config::CELL_SIZE, r * config::CELL_SIZE);
            // בדיקה האם הכלי נמצא במצב תנועה (Move).
            if (anim.state == AnimationState::Move) {

                const PieceMove& move = *activeMove;
                double t = (move.durationMs > 0)
                               ? double(state.elapsedMs - move.startMs) / double(move.durationMs)
                               : 1.0;
                t = std::clamp(t, 0.0, 1.0);

                double row = move.from.row + (move.to.row - move.from.row) * t;
                double col = move.from.col + (move.to.col - move.from.col) * t;
                drawAt = cv::Point(boardX + static_cast<int>(col * config::CELL_SIZE),
                                    static_cast<int>(row * config::CELL_SIZE));
            }
            // ציור (הדבקה) של הספרייט של הכלי על גבי התמונה הראשית במיקום המחושב.
            overlayImage(image, sprite, drawAt);

            // If the piece is cooling down, draw the draining overlay on top
            if (auto remaining = restRemainingFraction(activeRest, state.elapsedMs)) {
                drawRestOverlay(image, boardX, r, c, *remaining);
            }
        }
    }

    
    if (state.selection.active) {
        if (auto selectedPiece = board.pieceAt(state.selection.cell)) {
            for (Position dest : legalDestinations(board, *selectedPiece)) {
                drawLegalMoveHighlight(image, boardX, dest);
            }
        }
    }

    for (int r = 0; r < rows; ++r) drawRankLabel(image, boardX, r, rows);
    for (int c = 0; c < cols; ++c) drawFileLabel(image, boardX, c, rows);

    // input the move to the match record panel.
    std::vector<MoveRecord> blackMoves, whiteMoves;
    for (const MoveRecord& rec : state.moveHistory) {
        (rec.color == Color::White ? whiteMoves : blackMoves).push_back(rec);
    }
    drawHistoryPanel(image, 0, config::HISTORY_PANEL_WIDTH, boardHeight,
                      "Black (Player B)", state.score.black, blackMoves, rows);
    drawHistoryPanel(image, boardX + boardWidth, config::HISTORY_PANEL_WIDTH, boardHeight,
                      "White (Player A)", state.score.white, whiteMoves, rows);

    // Drawn absolutely last so it sits on top of the board, pieces,
    // highlights, and both panels - once this shows, nothing else in the
    // frame should still read as interactive.
    if (state.gameOver) {
        drawGameOverOverlay(image, state.gameOver);
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

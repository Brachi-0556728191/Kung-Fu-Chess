#include "BoardParser.hpp"
#include "BoardFormat.hpp"
#include "../model/Piece.hpp"
#include <cctype>
#include <sstream>

std::string trim(const std::string& v) {
    size_t a = 0, b = v.size();
    while (a < b && std::isspace((unsigned char)v[a])) ++a;
    while (b > a && std::isspace((unsigned char)v[b - 1])) --b;
    return v.substr(a, b - a);
}

std::vector<std::string> splitWords(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) out.push_back(tok);
    return out;
}

Sections parseSections(const std::string& text) {
    Sections s;
    std::istringstream stream(text);
    std::string line;
    enum { NONE, BOARD, COMMANDS } where = NONE;

    while (std::getline(stream, line)) {
        std::string t = trim(line);
        if (t == io_format::SECTION_BOARD)    { where = BOARD;    continue; }
        if (t == io_format::SECTION_COMMANDS) { where = COMMANDS; continue; }
        if (t.empty())                        { continue; }

        if (where == BOARD)         s.boardLines.push_back(t);
        else if (where == COMMANDS) s.commandLines.push_back(t);
        else                        throw BoardError("UNRECOGNIZED_SECTION_LINE");
    }
    return s;
}

Board parseBoard(const std::vector<std::string>& boardLines) {
    std::vector<std::vector<std::string>> rawGrid;
    for (const auto& line : boardLines) rawGrid.push_back(splitWords(line));

    validateBoard(rawGrid);

    int rows = (int)rawGrid.size();
    int cols = rawGrid.empty() ? 0 : (int)rawGrid[0].size();
    Board board(rows, cols);

    int nextId = 1;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const std::string& tok = rawGrid[r][c];
            if (tok != EMPTY_TOKEN) {
                board.addPiece(pieceFromToken(tok, Position{r, c}, nextId));
                ++nextId;
            }
        }
    }
    return board;
}

bool isValidToken(const std::string& t) {
    if (t == EMPTY_TOKEN) return true;
    if (t.size() != 2) return false;
    if (t[0] != COLOR_WHITE_CHAR && t[0] != COLOR_BLACK_CHAR) return false;
    return isValidKindChar(t[1]);
}

void validateBoard(const std::vector<std::vector<std::string>>& rawGrid) {
    if (rawGrid.empty()) return;

    size_t expected = rawGrid[0].size();
    for (const auto& row : rawGrid)                 // structural check
        if (row.size() != expected) throw BoardError("ROW_WIDTH_MISMATCH");

    for (const auto& row : rawGrid)                 // token check
        for (const auto& tok : row)
            if (!isValidToken(tok)) throw BoardError("UNKNOWN_TOKEN");
}

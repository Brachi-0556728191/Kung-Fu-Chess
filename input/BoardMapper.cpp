#include "BoardMapper.hpp"

#include "../rules/config.hpp"

std::optional<Position> pixelToCell(int x, int y, const Board& board) {
    
    int boardX = x - config::HISTORY_PANEL_WIDTH;
    if (boardX < 0 || y < 0) return std::nullopt;

    int col = boardX / config::CELL_SIZE;
    int row = y / config::CELL_SIZE;

    if (!board.isInBounds(Position{row, col}))
        return std::nullopt;

    return Position{row, col};
}

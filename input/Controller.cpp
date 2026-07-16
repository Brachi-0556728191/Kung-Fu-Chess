#include "Controller.hpp"

#include <optional>

#include "../model/Board.hpp"
#include "BoardMapper.hpp"
#include "../engine/GameEngine.hpp"

namespace Controller {

    void click(GameState& st, int x, int y) {
        auto cell = pixelToCell(x, y, st.board);
        if (!cell) {
            if (st.selection.active) st.selection = Selection{};
            return;
        }

        int row = cell->row;
        int col = cell->col;

        std::optional<Piece> clicked = st.board.pieceAt(*cell);

        if (st.selection.active) {
            std::optional<Piece> selected = st.board.pieceAt(st.selection.cell);
            bool sameSide = clicked && selected && clicked->color == selected->color;
            if (sameSide) {
                st.selection = {true, {row, col}, st.elapsedMs};   // reselect
            } else {
                sendMove(st, row, col);                            // complete: move or capture
            }
            return;
        }

        if (clicked) {
            st.selection = {true, {row, col}, st.elapsedMs};       // open a fresh selection
        }
    }

    void jump(GameState& st, int x, int y) {
        auto cell = pixelToCell(x, y, st.board);
        if (!cell) return;

        std::optional<Piece> piece = st.board.pieceAt(*cell);
        if (!piece) return;

        startJump(st, *cell);
    }

}

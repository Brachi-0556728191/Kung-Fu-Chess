#include "BoardPrinter.hpp"

#include <sstream>

#include "../model/Piece.hpp"

std::string formatBoard(const Board& b) {
    std::ostringstream out;
    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            if (c) out << ' ';
            auto piece = b.pieceAt(Position{r, c});
            out << (piece ? tokenFromPiece(*piece) : EMPTY_TOKEN);
        }
        out << '\n';
    }
    return out.str();
}

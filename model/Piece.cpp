#include "Piece.hpp"

#include "Board.hpp"

bool isValidKindChar(char c) {
    switch (c) {
        case 'K': case 'Q': case 'R':
        case 'B': case 'N': case 'P': return true;
        default: return false;
    }
}

Kind kindFromChar(char c) {
    switch (c) {
        case 'K': return Kind::King;
        case 'Q': return Kind::Queen;
        case 'R': return Kind::Rook;
        case 'B': return Kind::Bishop;
        case 'N': return Kind::Knight;
        case 'P': return Kind::Pawn;
        default:  throw PieceError("INVALID_PIECE_TOKEN");
    }
}

char charFromKind(Kind k) {
    switch (k) {
        case Kind::King:   return 'K';
        case Kind::Queen:  return 'Q';
        case Kind::Rook:   return 'R';
        case Kind::Bishop: return 'B';
        case Kind::Knight: return 'N';
        case Kind::Pawn:   return 'P';
    }
    throw PieceError("INVALID_PIECE_TOKEN");
}

char colorToChar(Color c) {
    return (c == Color::White) ? COLOR_WHITE_CHAR : COLOR_BLACK_CHAR;
}

Color colorFromChar(char c) {
    if (c == COLOR_WHITE_CHAR) return Color::White;
    if (c == COLOR_BLACK_CHAR) return Color::Black;
    throw PieceError("INVALID_PIECE_TOKEN");
}

namespace {
    bool isValidPieceToken(const std::string& token) {
        if (token.size() != 2) return false;
        if (token[0] != COLOR_WHITE_CHAR && token[0] != COLOR_BLACK_CHAR) return false;
        return isValidKindChar(token[1]);
    }
}

Piece pieceFromToken(const std::string& token, Position at, int id) {
    if (!isValidPieceToken(token)) throw PieceError("INVALID_PIECE_TOKEN");

    Piece piece;
    piece.id    = id;
    piece.color = colorFromChar(colorOf(token));
    piece.kind  = kindFromChar(pieceOf(token));
    piece.cell  = at;
    piece.state = PieceState::Idle;
    return piece;
}

std::string tokenFromPiece(const Piece& piece) {
    std::string token(2, '?');
    token[0] = colorToChar(piece.color);
    token[1] = charFromKind(piece.kind);
    return token;
}

#pragma once

#include <stdexcept>
#include <string>

#include "Position.hpp"

enum class Color { White, Black };
enum class Kind  { King, Queen, Rook, Bishop, Knight, Pawn };
enum class PieceState { Idle, Moving, Captured };

struct Piece {
    int        id;
    Color      color;
    Kind       kind;
    Position   cell;
    PieceState state = PieceState::Idle;
    bool       hasMoved = false;
};

class PieceError : public std::runtime_error {
public:
    explicit PieceError(const std::string& code)
        : std::runtime_error(code), code_(code) {}
    const std::string& code() const { return code_; }
private:
    std::string code_;
};

bool isValidKindChar(char c);

Kind kindFromChar(char c);

char charFromKind(Kind k);

char colorToChar(Color c);

Color colorFromChar(char c);

inline const std::string EMPTY_TOKEN = ".";
inline constexpr char COLOR_WHITE_CHAR = 'w';
inline constexpr char COLOR_BLACK_CHAR = 'b';

Piece pieceFromToken(const std::string& token, Position at, int id);

std::string tokenFromPiece(const Piece& piece);

#pragma once

enum class RestKind { Short, Long };

struct PieceRest {
    int      pieceId;
    RestKind kind;
    long     startMs;
    long     untilMs;
};

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "../model/Board.hpp"

class BoardError : public std::runtime_error {
public:
    explicit BoardError(const std::string& code)
        : std::runtime_error(code), code_(code) {}
    const std::string& code() const { return code_; }
private:
    std::string code_;
};

struct Sections {
    std::vector<std::string> boardLines;
    std::vector<std::string> commandLines;
};

std::string trim(const std::string& v);

std::vector<std::string> splitWords(const std::string& line);

Sections parseSections(const std::string& text);

Board parseBoard(const std::vector<std::string>& boardLines);

bool isValidToken(const std::string& t);

void validateBoard(const std::vector<std::vector<std::string>>& rawGrid);

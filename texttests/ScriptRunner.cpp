#include "ScriptRunner.hpp"

#include <iostream>
#include <sstream>

#include "../input/Controller.hpp"
#include "../engine/GameEngine.hpp"
#include "../io/BoardParser.hpp"
#include "../io/BoardPrinter.hpp"

namespace ScriptRunner {

    void run(const std::vector<std::string>& commands, GameState& st) {
        for (const auto& command : commands) {
            std::istringstream ss(command);
            std::string verb;
            ss >> verb;

            if (verb == "click") {
                int x, y;
                ss >> x >> y;
                Controller::click(st, x, y);
            } else if (verb == "jump") {
                int x, y;
                ss >> x >> y;
                Controller::jump(st, x, y);
            } else if (verb == "wait") {
                long ms; ss >> ms;
                handleWait(st, ms);
            } else if (verb == "print") {
                std::string rest; std::getline(ss, rest);
                if (trim(rest) == "board") std::cout << formatBoard(st.board);
            }
        }
    }

}

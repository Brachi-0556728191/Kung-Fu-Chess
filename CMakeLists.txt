cmake_minimum_required(VERSION 3.14)
project(chess_game CXX)

# ----- OpenCV -----
# set(OpenCV_DIR "C:/Users/PC/Documents/opencv/build/x64/vc16/lib")
 set(OpenCV_DIR "C:/Users/PC/Documents/opencv/build")

find_package(OpenCV REQUIRED)

# ----- S1: WebSocket echo proof-of-pipe -----
find_package(ixwebsocket CONFIG REQUIRED)

# ----- S3: JSON serialization for the message protocol -----
find_package(nlohmann_json CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(chess_game
    main.cpp
    model/Board.cpp
    model/Piece.cpp
    model/Position.cpp
    rules/PieceRules.cpp
    rules/RuleEngine.cpp
    realtime/RealTimeArbiter.cpp
    input/BoardMapper.cpp
    input/Controller.cpp
    engine/GameEngine.cpp
    texttests/ScriptRunner.cpp
    io/BoardParser.cpp
    io/BoardPrinter.cpp
    view/image_view.cpp
    view/img.cpp
    view/AnimationState.cpp

)

target_include_directories(chess_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(chess_game
    PRIVATE
        ${OpenCV_LIBS}
)

target_compile_definitions(chess_game
    PRIVATE
        CHESS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)


add_executable(server_echo server_echo.cpp)
target_include_directories(server_echo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server_echo PRIVATE ixwebsocket::ixwebsocket bcrypt nlohmann_json::nlohmann_json)

add_executable(client_echo client_echo.cpp)
target_include_directories(client_echo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client_echo PRIVATE ixwebsocket::ixwebsocket bcrypt nlohmann_json::nlohmann_json)


add_executable(server_main
    server_main.cpp
    model/Board.cpp
    model/Piece.cpp
    model/Position.cpp
    rules/PieceRules.cpp
    rules/RuleEngine.cpp
    realtime/RealTimeArbiter.cpp
    input/BoardMapper.cpp
    input/Controller.cpp
    engine/GameEngine.cpp
    io/BoardParser.cpp
)
target_include_directories(server_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server_main PRIVATE ixwebsocket::ixwebsocket bcrypt nlohmann_json::nlohmann_json)


add_executable(client_main
    client_main.cpp
    model/Board.cpp
    model/Piece.cpp
    model/Position.cpp
    rules/PieceRules.cpp
    rules/RuleEngine.cpp
    realtime/RealTimeArbiter.cpp
    input/BoardMapper.cpp
    io/BoardParser.cpp
    view/image_view.cpp
    view/img.cpp
    view/AnimationState.cpp
)
target_include_directories(client_main
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(client_main
    PRIVATE
        ${OpenCV_LIBS}
        ixwebsocket::ixwebsocket
        bcrypt
        nlohmann_json::nlohmann_json
)
target_compile_definitions(client_main
    PRIVATE
        CHESS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

option(BUILD_TESTS "Build the doctest test suite" ON)

if(BUILD_TESTS)
    enable_testing()

    add_executable(chess_tests
        tests/doctest_main.cpp
        tests/Config.tests.cpp
        tests/Board.tests.cpp
        tests/Movement.tests.cpp
        tests/Engine.tests.cpp
        tests/Piece.tests.cpp
        model/Board.cpp
        model/Piece.cpp
        model/Position.cpp
        rules/PieceRules.cpp
        rules/RuleEngine.cpp
        realtime/RealTimeArbiter.cpp
        input/BoardMapper.cpp
        input/Controller.cpp
        engine/GameEngine.cpp
        texttests/ScriptRunner.cpp
        io/BoardParser.cpp
        io/BoardPrinter.cpp
        
    )
    target_include_directories(chess_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        third_party
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(chess_tests
    PRIVATE
        ${OpenCV_LIBS}
)
    add_test(NAME chess_tests COMMAND chess_tests)

    option(ENABLE_COVERAGE "Build chess_tests with coverage instrumentation" OFF)

    if(ENABLE_COVERAGE)
        target_compile_options(chess_tests PRIVATE --coverage -O0 -g)
        target_link_options(chess_tests PRIVATE --coverage)
    endif()
endif()

cmake_minimum_required(VERSION 3.20)
project(fathom)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fathom src/main.cpp src/config.cpp src/order_book.cpp src/lobster_parser.cpp src/replay.cpp src/metrics.cpp)
target_include_directories(fathom PRIVATE src)

find_package(Catch2 3 REQUIRED)
add_executable(fathom_tests
    tests/test_order_book.cpp
    tests/test_metrics.cpp
    tests/test_lobster_parser.cpp
    tests/test_naive_strategy.cpp
    src/config.cpp
    src/order_book.cpp
    src/lobster_parser.cpp
    src/replay.cpp
    src/metrics.cpp
)
target_include_directories(fathom_tests PRIVATE src)
target_link_libraries(fathom_tests PRIVATE Catch2::Catch2WithMain)
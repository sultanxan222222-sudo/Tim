cmake_minimum_required(VERSION 3.16)
project(whats_next_history_gate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(whats_next_history src/history_gate.cpp)
target_include_directories(whats_next_history PUBLIC include)

add_executable(history_gate_test tests/history_gate_test.cpp)
target_link_libraries(history_gate_test PRIVATE whats_next_history)

enable_testing()
add_test(NAME history_gate_test COMMAND history_gate_test)

cmake_minimum_required(VERSION 3.16)
project(whats_next_mini_dlss LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(whats_next_history src/history_gate.cpp src/motion_estimator.cpp src/frame_generation_policy.cpp src/frame_generation_controller.cpp src/frame_generation_bridge.cpp src/resonance_frame_adapter.cpp)
target_include_directories(whats_next_history PUBLIC include)

option(WHATS_NEXT_ENABLE_WARNINGS "Enable strict compiler warnings for the host reference core" ON)
if(WHATS_NEXT_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(whats_next_history PRIVATE /W4 /permissive-)
    else()
        target_compile_options(whats_next_history PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

add_executable(history_gate_test tests/history_gate_test.cpp)
target_link_libraries(history_gate_test PRIVATE whats_next_history)

add_executable(motion_estimator_test tests/motion_estimator_test.cpp)
target_link_libraries(motion_estimator_test PRIVATE whats_next_history)

add_executable(frame_generation_policy_test tests/frame_generation_policy_test.cpp)
target_link_libraries(frame_generation_policy_test PRIVATE whats_next_history)

add_executable(frame_generation_controller_test tests/frame_generation_controller_test.cpp)
target_link_libraries(frame_generation_controller_test PRIVATE whats_next_history)

add_executable(frame_generation_bridge_test tests/frame_generation_bridge_test.cpp)
target_link_libraries(frame_generation_bridge_test PRIVATE whats_next_history)

# These tests use assert() as their assertion mechanism. CMake's Release
# configuration normally defines NDEBUG, which would silently compile every
# assertion out. Explicitly keep assertions active for all test executables.
set(WHATS_NEXT_TEST_TARGETS
    history_gate_test
    motion_estimator_test
    frame_generation_policy_test
    frame_generation_controller_test
    frame_generation_bridge_test
)
foreach(target IN LISTS WHATS_NEXT_TEST_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endforeach()

enable_testing()
add_test(NAME history_gate_test COMMAND history_gate_test)
add_test(NAME motion_estimator_test COMMAND motion_estimator_test)
add_test(NAME frame_generation_policy_test COMMAND frame_generation_policy_test)
add_test(NAME frame_generation_controller_test COMMAND frame_generation_controller_test)
add_test(NAME frame_generation_bridge_test COMMAND frame_generation_bridge_test)

option(WHATS_NEXT_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer on supported host compilers" OFF)
if(WHATS_NEXT_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target
        whats_next_history
        history_gate_test
        motion_estimator_test
        frame_generation_policy_test
        frame_generation_controller_test
        frame_generation_bridge_test)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

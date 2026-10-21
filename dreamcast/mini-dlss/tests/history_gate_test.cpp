#include "whats_next/history_gate.hpp"

#include <cassert>

using namespace whats_next;

int main() {
    HistoryGate gate;
    FrameContract a{640, 480, PixelFormat::RGBA8, 1, 1, 1, false, false};
    FrameContract b{640, 480, PixelFormat::RGBA8, 2, 1, 1, false, false};

    assert(gate.evaluate(a) == HistoryDecision::RejectNoPrevious);
    gate.commit(a);
    assert(gate.evaluate(b) == HistoryDecision::Accept);

    auto resize = b;
    resize.frame_id = 3;
    resize.width = 1280;
    assert(gate.evaluate(resize) == HistoryDecision::RejectResourceChanged);

    auto new_resources = b;
    new_resources.frame_id = 3;
    new_resources.resource_generation = 2;
    assert(gate.evaluate(new_resources) == HistoryDecision::RejectResourceChanged);

    auto new_game = b;
    new_game.frame_id = 3;
    new_game.game_session = 2;
    assert(gate.evaluate(new_game) == HistoryDecision::RejectSessionChanged);

    auto dropped = b;
    dropped.frame_id = 4;
    assert(gate.evaluate(dropped) == HistoryDecision::RejectDiscontinuity);

    auto invalid = b;
    invalid.width = 0;
    assert(gate.evaluate(invalid) == HistoryDecision::RejectInvalidCurrent);

    gate.reset();
    assert(gate.evaluate(b) == HistoryDecision::RejectNoPrevious);
    return 0;
}

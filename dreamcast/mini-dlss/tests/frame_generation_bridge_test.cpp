#include "whats_next/frame_generation_bridge.hpp"

#include <cassert>

using namespace whats_next;

int main() {
    FrameGenerationBridge bridge;

    auto decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    assert(bridge.current_frame_id() == 1);
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 2);

    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    // Generated images are not separately assigned source frame IDs.
    assert(bridge.current_frame_id() == 2);
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 3);

    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);

    bridge.reset();
    assert(bridge.current_frame_id() == 1);
    decision = bridge.begin_real_frame(
        true, 0, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();

    // Cadence mismatch and disabled mode must never claim a generated frame.
    decision = bridge.begin_real_frame(
        false, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::BypassDisabled);
    bridge.commit_real_frame();
    return 0;
}

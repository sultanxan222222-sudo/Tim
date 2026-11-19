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

    // A second commit without a matching begin must be a no-op.
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 2);

    // A second begin without a matching commit/abort is a lifecycle violation:
    // reset history rather than allowing the abandoned frame to be committed late.
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 3);

    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    // Generated images are not separately assigned source frame IDs.
    assert(bridge.current_frame_id() == 3);
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 4);

    // Explicitly aborted source frames do not advance IDs or poison accepted history.
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    bridge.abort_real_frame();
    assert(bridge.current_frame_id() == 4);
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    bridge.commit_real_frame();
    assert(bridge.current_frame_id() == 5);

    // Aborting a frame that observed a resource change keeps the last accepted
    // history; retrying the changed resource still correctly rejects that history.
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.abort_real_frame();
    assert(bridge.current_frame_id() == 5);
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 1, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    bridge.commit_real_frame();

    // A game/session switch must also invalidate temporal history.
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 2, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 2, 2, {2});
    assert(decision == FrameGenerationDecision::GenerateIntermediate);

    bridge.reset();
    assert(bridge.current_frame_id() == 1);
    decision = bridge.begin_real_frame(
        true, 0, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();

    // Reset between begin and commit must invalidate the pending source frame.
    // A late commit or abort from the old renderer generation cannot seed history.
    bridge.reset();
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 10, 10, {2});
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    bridge.reset();
    bridge.commit_real_frame();
    bridge.abort_real_frame();
    assert(bridge.current_frame_id() == 1);
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 10, 10, {2});
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    bridge.commit_real_frame();

    // Cadence mismatch and disabled mode must never claim a generated frame.
    decision = bridge.begin_real_frame(
        true, 640, 480, PixelFormat::RGBA8, 1, 1, {1});
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    bridge.commit_real_frame();
    decision = bridge.begin_real_frame(
        false, 640, 480, PixelFormat::RGBA8, 1, 1, {2});
    assert(decision == FrameGenerationDecision::BypassDisabled);
    bridge.commit_real_frame();
    return 0;
}

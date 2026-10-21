#include "whats_next/frame_generation_controller.hpp"

#include <cassert>

using namespace whats_next;

int main() {
    FrameGenerationController controller;
    const FrameContract f1{640, 480, PixelFormat::RGBA8, 1, 1, 1, false, false};
    const FrameContract f2{640, 480, PixelFormat::RGBA8, 2, 1, 1, false, false};
    const FrameContract f3{640, 480, PixelFormat::RGBA8, 3, 1, 1, false, false};

    // The first real frame seeds history; it must never be interpolated.
    assert(controller.decide(true, f1, {2}) ==
           FrameGenerationDecision::PresentRealFirstFrame);
    controller.commit_real_frame(f1);

    // Only consecutive real frames at the intended 30->60 cadence are eligible.
    assert(controller.decide(true, f2, {2}) ==
           FrameGenerationDecision::GenerateIntermediate);
    controller.commit_real_frame(f2);

    // A dropped frame fails closed; commit the real frame so the next pair can recover.
    auto dropped = f3;
    dropped.frame_id = 4;
    assert(controller.decide(true, dropped, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);
    controller.commit_real_frame(dropped);
    auto next = dropped;
    next.frame_id = 5;
    assert(controller.decide(true, next, {2}) ==
           FrameGenerationDecision::GenerateIntermediate);

    // Resize/resource recreation and session changes are rejected until history is reseeded.
    auto resized = next;
    resized.frame_id = 6;
    resized.width = 1280;
    assert(controller.decide(true, resized, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    auto new_session = next;
    new_session.frame_id = 6;
    new_session.game_session = 2;
    assert(controller.decide(true, new_session, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    controller.reset();
    assert(controller.decide(true, f1, {2}) ==
           FrameGenerationDecision::PresentRealFirstFrame);

    // Disabling the feature always takes the original presentation path.
    assert(controller.decide(false, f2, {2}) ==
           FrameGenerationDecision::BypassDisabled);
    return 0;
}

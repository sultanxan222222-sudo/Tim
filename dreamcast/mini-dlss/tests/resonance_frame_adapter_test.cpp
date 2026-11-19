#include "whats_next/resonance_frame_adapter.hpp"

#include <cassert>

using namespace whats_next;

int main() {
    ResonanceFrameAdapter adapter;

    // Translate refresh interval counts into policy cadence. The first real
    // image only seeds history and must not invent a generated frame.
    auto decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    adapter.commit_source_frame();

    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.commit_source_frame();

    // A recording abort must preserve the last accepted source frame and reuse
    // the same source ID when the renderer retries this frame.
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.abort_source_frame();
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.commit_source_frame();

    // Resource generation change invalidates the old history; the next
    // accepted real frame seeds the replacement history.
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 2, 7, 2);
    assert(decision == FrameGenerationDecision::PresentRealHistoryRejected);
    adapter.commit_source_frame();
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 2, 7, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.commit_source_frame();

    // One refresh interval is not the intended 30-to-60 cadence.
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 2, 7, 1);
    assert(decision == FrameGenerationDecision::PresentRealCadenceMismatch);
    adapter.commit_source_frame();

    adapter.reset();
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 2, 8, 2);
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    adapter.commit_source_frame();

    // Disabled mode still commits real frames, so history recovers when enabled.
    decision = adapter.begin_source_frame(
        false, 1280, 720, PixelFormat::RGBA8, 2, 8, 2);
    assert(decision == FrameGenerationDecision::BypassDisabled);
    adapter.commit_source_frame();
    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 2, 8, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.commit_source_frame();
    return 0;
}

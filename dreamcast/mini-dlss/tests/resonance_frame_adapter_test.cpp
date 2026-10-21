#include "whats_next/resonance_frame_adapter.hpp"

#include <cassert>

using namespace whats_next;

int main() {
    ResonanceFrameAdapter adapter;

    // The adapter must translate the renderer's refresh interval count into
    // the policy cadence without inventing a generated frame for the first image.
    auto decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::PresentRealFirstFrame);
    adapter.commit_source_frame();

    decision = adapter.begin_source_frame(
        true, 1280, 720, PixelFormat::RGBA8, 1, 7, 2);
    assert(decision == FrameGenerationDecision::GenerateIntermediate);
    adapter.commit_source_frame();

    // A resource generation change invalidates the old history; the next
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

    // Disabled mode must bypass generation while still allowing source frames
    // to be committed so the renderer can recover when the toggle is re-enabled.
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

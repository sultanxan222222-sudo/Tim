#include "whats_next/frame_generation_policy.hpp"

namespace whats_next {

FrameGenerationDecision FrameGenerationPolicy::decide(
    bool enabled,
    const FrameContract& current,
    HistoryDecision history,
    PresentationCadence cadence) const noexcept {
    if (!enabled)
        return FrameGenerationDecision::BypassDisabled;

    // Invalid frames must never enter a temporal reconstruction pass.
    if (!is_valid(current))
        return FrameGenerationDecision::PresentRealHistoryRejected;

    if (history == HistoryDecision::RejectNoPrevious)
        return FrameGenerationDecision::PresentRealFirstFrame;

    if (history != HistoryDecision::Accept)
        return FrameGenerationDecision::PresentRealHistoryRejected;

    // M33 only interpolates when a 30 FPS source frame would otherwise be repeated
    // across two display refreshes. Never synthesize on unknown/other cadence.
    if (cadence.refresh_intervals != 2)
        return FrameGenerationDecision::PresentRealCadenceMismatch;

    return FrameGenerationDecision::GenerateIntermediate;
}

} // namespace whats_next

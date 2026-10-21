#pragma once

#include "whats_next/frame_generation_policy.hpp"
#include "whats_next/history_gate.hpp"

namespace whats_next {

// Stateful, backend-neutral coordinator for frame admission. The renderer asks
// decide() for each real source frame, then calls commit_real_frame() only after
// that source frame is accepted as the new history reference. Generated frames
// must never be committed as source history.
class FrameGenerationController {
public:
    FrameGenerationDecision decide(
        bool enabled,
        const FrameContract& current,
        PresentationCadence cadence) const noexcept;

    void commit_real_frame(const FrameContract& completed) noexcept;
    void reset() noexcept;

private:
    HistoryGate history_;
    FrameGenerationPolicy policy_;
};

} // namespace whats_next

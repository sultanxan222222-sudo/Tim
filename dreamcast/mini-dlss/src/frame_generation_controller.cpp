#include "whats_next/frame_generation_controller.hpp"

namespace whats_next {

FrameGenerationDecision FrameGenerationController::decide(
    bool enabled,
    const FrameContract& current,
    PresentationCadence cadence) const noexcept {
    return policy_.decide(enabled, current, history_.evaluate(current), cadence);
}

void FrameGenerationController::commit_real_frame(
    const FrameContract& completed) noexcept {
    history_.commit(completed);
}

void FrameGenerationController::reset() noexcept {
    history_.reset();
}

} // namespace whats_next

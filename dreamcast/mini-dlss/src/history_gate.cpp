#include "whats_next/history_gate.hpp"

namespace whats_next {

HistoryDecision HistoryGate::evaluate(const FrameContract& current) const noexcept {
    if (!is_valid(current))
        return HistoryDecision::RejectInvalidCurrent;

    if (!has_previous_)
        return HistoryDecision::RejectNoPrevious;

    if (current.game_session != previous_.game_session)
        return HistoryDecision::RejectSessionChanged;

    if (current.resource_generation != previous_.resource_generation ||
        current.width != previous_.width ||
        current.height != previous_.height ||
        current.format != previous_.format)
        return HistoryDecision::RejectResourceChanged;

    // This first version only accepts consecutive frames. If frames were
    // dropped, never blend stale history and introduce a visible ghost.
    if (previous_.frame_id == UINT64_MAX || current.frame_id != previous_.frame_id + 1)
        return HistoryDecision::RejectDiscontinuity;

    return HistoryDecision::Accept;
}

void HistoryGate::commit(const FrameContract& completed) noexcept {
    if (!is_valid(completed)) {
        reset();
        return;
    }
    previous_ = completed;
    has_previous_ = true;
}

void HistoryGate::reset() noexcept {
    previous_ = {};
    has_previous_ = false;
}

} // namespace whats_next

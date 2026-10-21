#pragma once

#include "whats_next/frame_contract.hpp"

namespace whats_next {

enum class HistoryDecision : std::uint8_t {
    RejectInvalidCurrent,
    RejectNoPrevious,
    RejectDiscontinuity,
    RejectResourceChanged,
    RejectSessionChanged,
    Accept
};

class HistoryGate {
public:
    HistoryDecision evaluate(const FrameContract& current) const noexcept;
    void commit(const FrameContract& completed) noexcept;
    void reset() noexcept;

private:
    FrameContract previous_{};
    bool has_previous_ = false;
};

} // namespace whats_next

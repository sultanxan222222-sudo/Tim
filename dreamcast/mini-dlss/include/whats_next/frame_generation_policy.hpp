#pragma once

#include "whats_next/frame_contract.hpp"
#include "whats_next/history_gate.hpp"

#include <cstdint>

namespace whats_next {

// Scheduling policy inspired by M33's optional 30->60 presentation interpolation.
// It deliberately does not own Vulkan resources or assume PPSSPP/Flycast types.
enum class FrameGenerationDecision : std::uint8_t {
    BypassDisabled,
    PresentRealFirstFrame,
    PresentRealCadenceMismatch,
    PresentRealHistoryRejected,
    GenerateIntermediate
};

struct PresentationCadence {
    // Number of display refresh intervals between the previous and current real frame.
    // M33's 30->60 case is 2 on a 60 Hz display.
    std::uint32_t refresh_intervals = 0;
};

class FrameGenerationPolicy {
public:
    FrameGenerationDecision decide(
        bool enabled,
        const FrameContract& current,
        HistoryDecision history,
        PresentationCadence cadence) const noexcept;
};

} // namespace whats_next

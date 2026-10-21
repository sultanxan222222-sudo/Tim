#pragma once

#include "whats_next/frame_generation_bridge.hpp"

#include <cstdint>

namespace whats_next {

// Thin lifecycle adapter for the existing Resonance Vulkan frame generator.
// It decides whether the existing GPU interpolation pass may run; it does not
// allocate Vulkan objects and does not replace FrameGenerator::Prepare/Draw.
class ResonanceFrameAdapter {
public:
    FrameGenerationDecision begin_source_frame(
        bool enabled,
        std::uint32_t width,
        std::uint32_t height,
        PixelFormat format,
        std::uint64_t resource_generation,
        std::uint64_t game_session,
        std::uint32_t refresh_intervals) noexcept;

    // Call after the renderer has accepted the source frame for presentation.
    void commit_source_frame() noexcept;

    // Call on swapchain/image recreation, game/session change, or renderer teardown.
    void reset() noexcept;

private:
    FrameGenerationBridge bridge_;
};

} // namespace whats_next

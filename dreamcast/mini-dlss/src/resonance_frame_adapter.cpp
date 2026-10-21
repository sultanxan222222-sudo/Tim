#include "whats_next/resonance_frame_adapter.hpp"

namespace whats_next {

FrameGenerationDecision ResonanceFrameAdapter::begin_source_frame(
    bool enabled,
    std::uint32_t width,
    std::uint32_t height,
    std::uint64_t resource_generation,
    std::uint64_t game_session,
    std::uint32_t refresh_intervals) noexcept {
    return bridge_.begin_real_frame(
        enabled,
        width,
        height,
        PixelFormat::RGBA8,
        resource_generation,
        game_session,
        PresentationCadence{refresh_intervals});
}

void ResonanceFrameAdapter::commit_source_frame() noexcept {
    bridge_.commit_real_frame();
}

void ResonanceFrameAdapter::reset() noexcept {
    bridge_.reset();
}

} // namespace whats_next

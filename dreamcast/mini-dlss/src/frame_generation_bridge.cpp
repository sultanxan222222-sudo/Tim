#include "whats_next/frame_generation_bridge.hpp"

#include <limits>

namespace whats_next {

FrameGenerationDecision FrameGenerationBridge::begin_real_frame(
    bool enabled,
    std::uint32_t width,
    std::uint32_t height,
    PixelFormat format,
    std::uint64_t resource_generation,
    std::uint64_t game_session,
    PresentationCadence cadence) noexcept {
    pending_valid_ = false;
    if (next_frame_id_ == 0 || next_frame_id_ == std::numeric_limits<std::uint64_t>::max()) {
        // Avoid wrapping IDs and accidentally treating stale history as consecutive.
        controller_.reset();
        next_frame_id_ = 1;
    }

    pending_ = FrameContract{
        width, height, format, next_frame_id_,
        resource_generation, game_session, false, false
    };
    pending_valid_ = true;
    return controller_.decide(enabled, pending_, cadence);
}

void FrameGenerationBridge::commit_real_frame() noexcept {
    if (!pending_valid_)
        return;
    controller_.commit_real_frame(pending_);
    pending_valid_ = false;
    ++next_frame_id_;
}

void FrameGenerationBridge::reset() noexcept {
    controller_.reset();
    pending_ = {};
    pending_valid_ = false;
    next_frame_id_ = 1;
}

} // namespace whats_next

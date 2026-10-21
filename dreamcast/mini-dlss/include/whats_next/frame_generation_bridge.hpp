#pragma once

#include "whats_next/frame_generation_controller.hpp"

#include <cstdint>

namespace whats_next {

// Small renderer-facing bridge. The backend supplies stable session/resource
// generations and real-frame dimensions/format; this class owns the monotonically
// increasing ID for real source frames. Repeated vblanks and generated frames must
// not call begin_real_frame().
class FrameGenerationBridge {
public:
    FrameGenerationDecision begin_real_frame(
        bool enabled,
        std::uint32_t width,
        std::uint32_t height,
        PixelFormat format,
        std::uint64_t resource_generation,
        std::uint64_t game_session,
        PresentationCadence cadence) noexcept;

    // Call only after the real source frame has been accepted by the renderer.
    // Call for real frames even when generation was rejected, so history can recover.
    void commit_real_frame() noexcept;

    // Call on renderer teardown, game switch, or any resource/session reset.
    void reset() noexcept;

    std::uint64_t current_frame_id() const noexcept { return next_frame_id_; }

private:
    FrameGenerationController controller_;
    FrameContract pending_{};
    std::uint64_t next_frame_id_ = 1;
    bool pending_valid_ = false;
};

} // namespace whats_next

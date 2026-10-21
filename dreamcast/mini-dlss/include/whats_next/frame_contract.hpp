#pragma once

#include <cstdint>

namespace whats_next {

// Backend-neutral description of one completed emulator frame.
// Vulkan handles are deliberately kept out of this contract so the
// scheduling/history logic can be host-tested without the PS4 SDK.
enum class PixelFormat : std::uint8_t {
    Unknown,
    RGBA8,
    BGRA8,
    RGBA16F
};

struct FrameContract {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::Unknown;
    std::uint64_t frame_id = 0;
    std::uint64_t resource_generation = 0;
    std::uint64_t game_session = 0;
    bool has_depth = false;
    bool has_motion_vectors = false;
};

enum class StageResult : std::uint8_t {
    Skipped,
    Processed,
    InvalidInput,
    BackendUnavailable,
    Failed
};

constexpr bool is_valid(const FrameContract& frame) noexcept {
    return frame.width > 0 &&
           frame.height > 0 &&
           frame.format != PixelFormat::Unknown &&
           frame.frame_id > 0 &&
           frame.resource_generation > 0 &&
           frame.game_session > 0;
}

} // namespace whats_next

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace whats_next {

// CPU reference implementation. Inputs are tightly packed 8-bit luminance images.
// Vectors describe the displacement from the previous frame to the current frame.
struct GrayImageView {
    const std::uint8_t* pixels = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t stride = 0;
};

struct MotionVector {
    std::int16_t dx = 0;
    std::int16_t dy = 0;
    float confidence = 0.0f;
    bool valid = false;
};

struct MotionConfig {
    std::uint32_t block_size = 8;
    std::uint32_t search_radius = 4;
    std::uint32_t sample_step = 2;
    float max_mean_absolute_error = 18.0f;
    float minimum_texture_variance = 12.0f;
};

struct MotionField {
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::vector<MotionVector> vectors;
    bool valid = false;
};

// Finds coarse block motion by bounded SAD matching. This is a conservative
// CPU baseline for tests and future backend comparison, not a GPU-ready pass.
MotionField estimate_block_motion(
    GrayImageView previous,
    GrayImageView current,
    const MotionConfig& config = {});

} // namespace whats_next

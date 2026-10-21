#include "whats_next/motion_estimator.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

using namespace whats_next;

int main() {
    constexpr std::uint32_t w = 32, h = 24;
    std::vector<std::uint8_t> previous(w * h, 0), current(w * h, 0);

    // Distinct, textured first block.
    for (std::uint32_t y = 0; y < 8; ++y)
        for (std::uint32_t x = 0; x < 8; ++x)
            previous[y * w + x] = static_cast<std::uint8_t>((x * 31 + y * 17 + x * y * 3) & 255);

    // Shift the texture two pixels right in the current frame.
    for (std::uint32_t y = 0; y < 8; ++y)
        for (std::uint32_t x = 2; x < 10; ++x)
            current[y * w + x] = previous[y * w + (x - 2)];

    MotionConfig cfg;
    cfg.block_size = 8;
    cfg.search_radius = 4;
    cfg.sample_step = 1;
    cfg.max_mean_absolute_error = 30.0f;
    cfg.minimum_texture_variance = 1.0f;
    auto field = estimate_block_motion({previous.data(), w, h, w},
                                       {current.data(), w, h, w}, cfg);
    assert(field.valid);
    assert(field.columns == 4 && field.rows == 3);
    assert(field.vectors.size() == 12);
    assert(field.vectors[0].valid);
    assert(field.vectors[0].dx == 2);
    assert(field.vectors[0].dy == 0);

    // Flat regions are deliberately invalid: no trustworthy motion signal.
    std::vector<std::uint8_t> flat(w * h, 128);
    auto flat_field = estimate_block_motion({flat.data(), w, h, w},
                                            {flat.data(), w, h, w}, cfg);
    assert(flat_field.valid);
    assert(!flat_field.vectors[0].valid);

    // Mismatched dimensions and invalid stride must fail closed.
    auto mismatch = estimate_block_motion({previous.data(), w, h, w},
                                          {current.data(), w - 1, h, w - 1}, cfg);
    assert(!mismatch.valid);
    auto bad_stride = estimate_block_motion({previous.data(), w, h, w - 1},
                                             {current.data(), w, h, w}, cfg);
    assert(!bad_stride.valid);
    // Padded row stride must be respected; padding bytes are intentionally noisy.
    constexpr std::size_t padded_stride = w + 7;
    std::vector<std::uint8_t> padded_prev(padded_stride * h, 0xEE);
    std::vector<std::uint8_t> padded_curr(padded_stride * h, 0xDD);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            padded_prev[y * padded_stride + x] = previous[y * w + x];
            padded_curr[y * padded_stride + x] = current[y * w + x];
        }
    }
    auto padded_field = estimate_block_motion(
        {padded_prev.data(), w, h, padded_stride},
        {padded_curr.data(), w, h, padded_stride}, cfg);
    assert(padded_field.valid);
    assert(padded_field.vectors[0].valid);
    assert(padded_field.vectors[0].dx == 2);
    assert(padded_field.vectors[0].dy == 0);

    // A scene-wide unrelated image should not be mistaken for a good match.
    std::vector<std::uint8_t> unrelated(w * h);
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x)
            unrelated[y * w + x] = static_cast<std::uint8_t>((x * 13 + y * 29 + 71) & 255);
    cfg.max_mean_absolute_error = 0.0f;
    auto cut_field = estimate_block_motion(
        {previous.data(), w, h, w}, {unrelated.data(), w, h, w}, cfg);
    assert(cut_field.valid);
    assert(!cut_field.vectors[0].valid);

    // Invalid configuration must fail closed rather than allocate/scan.
    cfg.sample_step = 0;
    auto invalid_config = estimate_block_motion(
        {previous.data(), w, h, w}, {current.data(), w, h, w}, cfg);
    assert(!invalid_config.valid);

    // NaN and infinity are not meaningful thresholds; reject them explicitly.
    cfg.sample_step = 1;
    cfg.max_mean_absolute_error = std::numeric_limits<float>::quiet_NaN();
    auto nan_threshold = estimate_block_motion(
        {previous.data(), w, h, w}, {current.data(), w, h, w}, cfg);
    assert(!nan_threshold.valid);
    cfg.max_mean_absolute_error = 18.0f;
    cfg.minimum_texture_variance = std::numeric_limits<float>::infinity();
    auto infinite_variance = estimate_block_motion(
        {previous.data(), w, h, w}, {current.data(), w, h, w}, cfg);
    assert(!infinite_variance.valid);

    // A null image pointer is invalid even when dimensions look plausible.
    auto null_input = estimate_block_motion(
        {nullptr, w, h, w}, {current.data(), w, h, w}, cfg);
    assert(!null_input.valid);
    return 0;
}

#include "whats_next/motion_estimator.hpp"

#include <cassert>
#include <cstdint>
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
    return 0;
}

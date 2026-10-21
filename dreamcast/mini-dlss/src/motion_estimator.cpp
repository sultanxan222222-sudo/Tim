#include "whats_next/motion_estimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cmath>

namespace whats_next {
namespace {

bool valid_image(const GrayImageView& image) noexcept {
    return image.pixels && image.width > 0 && image.height > 0 &&
           image.stride >= image.width;
}

double block_variance(const GrayImageView& image, std::uint32_t x,
                      std::uint32_t y, std::uint32_t size,
                      std::uint32_t step) noexcept {
    double sum = 0.0, sum_sq = 0.0;
    std::uint64_t count = 0;
    for (std::uint32_t by = 0; by < size; by += step) {
        for (std::uint32_t bx = 0; bx < size; bx += step) {
            const double p = image.pixels[(y + by) * image.stride + x + bx];
            sum += p;
            sum_sq += p * p;
            ++count;
        }
    }
    if (!count) return 0.0;
    const double mean = sum / static_cast<double>(count);
    return sum_sq / static_cast<double>(count) - mean * mean;
}

double sad(const GrayImageView& a, const GrayImageView& b,
           std::uint32_t ax, std::uint32_t ay,
           std::uint32_t bx, std::uint32_t by,
           std::uint32_t size, std::uint32_t step) noexcept {
    double error = 0.0;
    std::uint64_t count = 0;
    for (std::uint32_t y = 0; y < size; y += step) {
        for (std::uint32_t x = 0; x < size; x += step) {
            const int av = a.pixels[(ay + y) * a.stride + ax + x];
            const int bv = b.pixels[(by + y) * b.stride + bx + x];
            error += std::abs(av - bv);
            ++count;
        }
    }
    return count ? error / static_cast<double>(count)
                 : std::numeric_limits<double>::infinity();
}

} // namespace

MotionField estimate_block_motion(GrayImageView previous,
                                  GrayImageView current,
                                  const MotionConfig& config) {
    MotionField field;
    if (!valid_image(previous) || !valid_image(current) ||
        previous.width != current.width || previous.height != current.height ||
        config.block_size < 4 || config.search_radius > 32 ||
        config.sample_step == 0 || config.sample_step > config.block_size ||
        config.block_size > previous.width || config.block_size > previous.height ||
        !std::isfinite(config.max_mean_absolute_error) ||
        !std::isfinite(config.minimum_texture_variance) ||
        config.max_mean_absolute_error < 0.0f ||
        config.minimum_texture_variance < 0.0f) {
        return field;
    }

    const std::uint32_t size = config.block_size;
    field.columns = previous.width / size;
    field.rows = previous.height / size;
    if (!field.columns || !field.rows) return field;
    field.vectors.resize(static_cast<std::size_t>(field.columns) * field.rows);

    for (std::uint32_t gy = 0; gy < field.rows; ++gy) {
        for (std::uint32_t gx = 0; gx < field.columns; ++gx) {
            const std::uint32_t x = gx * size, y = gy * size;
            MotionVector result;
            if (block_variance(previous, x, y, size, config.sample_step) <
                config.minimum_texture_variance) {
                field.vectors[static_cast<std::size_t>(gy) * field.columns + gx] = result;
                continue;
            }

            double best = std::numeric_limits<double>::infinity();
            double second = std::numeric_limits<double>::infinity();
            int best_dx = 0, best_dy = 0;
            const int radius = static_cast<int>(config.search_radius);
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int cx = static_cast<int>(x) + dx;
                    const int cy = static_cast<int>(y) + dy;
                    if (cx < 0 || cy < 0 ||
                        cx + static_cast<int>(size) > static_cast<int>(current.width) ||
                        cy + static_cast<int>(size) > static_cast<int>(current.height))
                        continue;
                    const double score = sad(previous, current, x, y,
                        static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cy),
                        size, config.sample_step);
                    if (score < best) {
                        second = best;
                        best = score;
                        best_dx = dx;
                        best_dy = dy;
                    } else if (score < second) {
                        second = score;
                    }
                }
            }

            if (best <= config.max_mean_absolute_error) {
                // Confidence combines absolute match quality with separation
                // from the runner-up; ambiguous repeated patterns score lower.
                const double quality = 1.0 - best /
                    std::max(1.0, static_cast<double>(config.max_mean_absolute_error));
                const double uniqueness = std::isfinite(second)
                    ? std::clamp((second - best) / std::max(1.0, second), 0.0, 1.0)
                    : 0.0;
                result.dx = static_cast<std::int16_t>(best_dx);
                result.dy = static_cast<std::int16_t>(best_dy);
                result.confidence = static_cast<float>(
                    std::clamp(0.75 * quality + 0.25 * uniqueness, 0.0, 1.0));
                result.valid = true;
            }
            field.vectors[static_cast<std::size_t>(gy) * field.columns + gx] = result;
        }
    }
    field.valid = true;
    return field;
}

} // namespace whats_next

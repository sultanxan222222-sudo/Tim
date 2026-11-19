#include "whats_next/gpu_timestamp_math.hpp"

#include <cmath>

namespace whats_next {

bool gpu_timestamp_delta_ms(std::uint64_t start,
                            std::uint64_t end,
                            std::uint32_t valid_bits,
                            double period_ns,
                            double& milliseconds) noexcept {
    milliseconds = 0.0;
    if (valid_bits == 0 || valid_bits > 64 ||
        !std::isfinite(period_ns) || period_ns <= 0.0 || period_ns > 1.0e9) {
        return false;
    }

    // Unsigned subtraction is defined modulo 2^64. For narrower GPU counters,
    // mask the result to their advertised valid-bit width to handle wraparound.
    std::uint64_t ticks = end - start;
    if (valid_bits < 64)
        ticks &= (std::uint64_t{1} << valid_bits) - 1;

    if (ticks == 0)
        return false;

    const double converted = static_cast<double>(ticks) * period_ns / 1.0e6;
    // Reject implausible values rather than feeding a broken timer into the
    // adaptive-quality controller.
    if (!std::isfinite(converted) || converted <= 0.0 || converted > 1000.0)
        return false;

    milliseconds = converted;
    return true;
}

} // namespace whats_next

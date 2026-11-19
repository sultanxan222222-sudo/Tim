#include "whats_next/gpu_timestamp_math.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

using whats_next::gpu_timestamp_delta_ms;

int main() {
    double ms = -1.0;

    // 1,000,000 ticks at 10 ns each = 10 ms.
    assert(gpu_timestamp_delta_ms(100, 1000100, 64, 10.0, ms));
    assert(std::abs(ms - 10.0) < 1e-9);

    // 32-bit timestamp wraparound is handled using timestampValidBits.
    assert(gpu_timestamp_delta_ms(0xFFFFFFF0ull, 0x10ull, 32, 1000.0, ms));
    assert(std::abs(ms - 0.032) < 1e-9);

    // The 64-bit path must not shift by 64 when constructing a mask.
    assert(gpu_timestamp_delta_ms(0xFFFFFFFFFFFFFFF0ull, 0x10ull, 64, 1000.0, ms));
    assert(std::abs(ms - 0.048) < 1e-9);

    // Unsupported and malformed measurements must fail closed.
    assert(!gpu_timestamp_delta_ms(1, 2, 0, 10.0, ms));
    assert(ms == 0.0);
    assert(!gpu_timestamp_delta_ms(1, 2, 65, 10.0, ms));
    assert(!gpu_timestamp_delta_ms(1, 2, 32, 0.0, ms));
    assert(!gpu_timestamp_delta_ms(1, 2, 32, std::numeric_limits<double>::quiet_NaN(), ms));
    assert(!gpu_timestamp_delta_ms(7, 7, 32, 10.0, ms));
    assert(!gpu_timestamp_delta_ms(0, 2000000000ull, 64, 1000.0, ms));
    assert(ms == 0.0);

    return 0;
}

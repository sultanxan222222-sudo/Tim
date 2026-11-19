#pragma once

#include <cstdint>

namespace whats_next {

// Convert two completed Vulkan timestamp-query values into milliseconds.
// valid_bits comes from the graphics queue family's timestampValidBits and
// period_ns from VkPhysicalDeviceLimits::timestampPeriod.
// Returns false for unsupported/invalid data and leaves milliseconds at 0.
bool gpu_timestamp_delta_ms(std::uint64_t start,
                            std::uint64_t end,
                            std::uint32_t valid_bits,
                            double period_ns,
                            double& milliseconds) noexcept;

} // namespace whats_next

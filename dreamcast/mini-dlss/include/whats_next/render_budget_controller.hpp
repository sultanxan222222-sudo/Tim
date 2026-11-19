#pragma once

#include <cstdint>

namespace whats_next {

enum class RenderQuality : std::uint8_t {
    Eco = 0,
    Balanced = 1,
    Quality = 2
};

struct RenderBudgetConfig {
    double smoothing_alpha = 0.20;
    double target_utilization = 0.85;
    double downgrade_threshold = 1.05;
    double upgrade_threshold = 0.72;
    std::uint32_t downgrade_samples = 8;
    std::uint32_t upgrade_samples = 120;
};

struct RenderQualityProfile {
    float sharpening_strength = 0.12f;
    float edge_threshold = 0.08f;
    bool temporal_effects_allowed = false;
};

class RenderBudgetController {
public:
    explicit RenderBudgetController(RenderBudgetConfig config = {}) noexcept;

    // Feed completed GPU frame time and the current display-frame budget.
    // Invalid or unavailable measurements leave the current tier unchanged.
    RenderQuality update(double gpu_time_ms, double frame_budget_ms,
                         bool measurement_valid = true) noexcept;

    void reset(RenderQuality initial = RenderQuality::Balanced) noexcept;

    RenderQuality quality() const noexcept { return quality_; }
    double smoothed_gpu_time_ms() const noexcept { return smoothed_gpu_time_ms_; }
    std::uint32_t valid_samples() const noexcept { return valid_samples_; }

    static RenderQualityProfile profile(RenderQuality quality) noexcept;

private:
    RenderBudgetConfig config_;
    RenderQuality quality_ = RenderQuality::Balanced;
    double smoothed_gpu_time_ms_ = 0.0;
    std::uint32_t valid_samples_ = 0;
    std::uint32_t over_budget_streak_ = 0;
    std::uint32_t under_budget_streak_ = 0;
};

} // namespace whats_next

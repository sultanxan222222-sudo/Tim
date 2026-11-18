#include "whats_next/render_budget_controller.hpp"

#include <algorithm>
#include <cmath>

namespace whats_next {

RenderBudgetController::RenderBudgetController(RenderBudgetConfig config) noexcept
    : config_(config) {
    if (!std::isfinite(config_.smoothing_alpha) ||
        config_.smoothing_alpha <= 0.0 || config_.smoothing_alpha > 1.0)
        config_.smoothing_alpha = 0.20;
    if (!std::isfinite(config_.target_utilization) ||
        config_.target_utilization <= 0.0 || config_.target_utilization > 1.0)
        config_.target_utilization = 0.85;
    if (!std::isfinite(config_.downgrade_threshold) ||
        config_.downgrade_threshold < 1.0)
        config_.downgrade_threshold = 1.05;
    if (!std::isfinite(config_.upgrade_threshold) ||
        config_.upgrade_threshold <= 0.0 ||
        config_.upgrade_threshold >= config_.downgrade_threshold)
        config_.upgrade_threshold = 0.72;
    config_.downgrade_samples = std::max<std::uint32_t>(1, config_.downgrade_samples);
    config_.upgrade_samples = std::max<std::uint32_t>(1, config_.upgrade_samples);
}

RenderQuality RenderBudgetController::update(double gpu_time_ms,
                                               double frame_budget_ms,
                                               bool measurement_valid) noexcept {
    if (!measurement_valid || !std::isfinite(gpu_time_ms) ||
        !std::isfinite(frame_budget_ms) || gpu_time_ms <= 0.0 ||
        frame_budget_ms <= 0.0 || gpu_time_ms > 1000.0 ||
        frame_budget_ms > 1000.0) {
        return quality_;
    }

    if (valid_samples_ == 0)
        smoothed_gpu_time_ms_ = gpu_time_ms;
    else
        smoothed_gpu_time_ms_ += config_.smoothing_alpha *
            (gpu_time_ms - smoothed_gpu_time_ms_);
    if (valid_samples_ < UINT32_MAX)
        ++valid_samples_;

    const double budget = frame_budget_ms * config_.target_utilization;
    const bool over = smoothed_gpu_time_ms_ > budget * config_.downgrade_threshold;
    const bool under = smoothed_gpu_time_ms_ < budget * config_.upgrade_threshold;

    if (over) {
        under_budget_streak_ = 0;
        if (over_budget_streak_ < UINT32_MAX)
            ++over_budget_streak_;
        if (over_budget_streak_ >= config_.downgrade_samples) {
            if (quality_ != RenderQuality::Eco)
                quality_ = static_cast<RenderQuality>(
                    static_cast<std::uint8_t>(quality_) - 1);
            over_budget_streak_ = 0;
        }
    } else if (under) {
        over_budget_streak_ = 0;
        if (under_budget_streak_ < UINT32_MAX)
            ++under_budget_streak_;
        if (under_budget_streak_ >= config_.upgrade_samples) {
            if (quality_ != RenderQuality::Quality)
                quality_ = static_cast<RenderQuality>(
                    static_cast<std::uint8_t>(quality_) + 1);
            under_budget_streak_ = 0;
        }
    } else {
        over_budget_streak_ = 0;
        under_budget_streak_ = 0;
    }
    return quality_;
}

void RenderBudgetController::reset(RenderQuality initial) noexcept {
    quality_ = initial;
    smoothed_gpu_time_ms_ = 0.0;
    valid_samples_ = 0;
    over_budget_streak_ = 0;
    under_budget_streak_ = 0;
}

RenderQualityProfile RenderBudgetController::profile(RenderQuality quality) noexcept {
    switch (quality) {
    case RenderQuality::Eco:
        return {0.08f, 0.11f, false};
    case RenderQuality::Quality:
        return {0.20f, 0.045f, true};
    case RenderQuality::Balanced:
    default:
        return {0.12f, 0.075f, false};
    }
}

} // namespace whats_next

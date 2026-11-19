#include "whats_next/render_budget_controller.hpp"

#include <cassert>
#include <cmath>

using namespace whats_next;

int main() {
    RenderBudgetConfig config;
    config.smoothing_alpha = 1.0;
    config.downgrade_samples = 3;
    config.upgrade_samples = 5;
    RenderBudgetController governor(config);

    // Invalid timing data must not perturb the selected quality.
    assert(governor.update(0.0, 16.667) == RenderQuality::Balanced);
    assert(governor.update(NAN, 16.667) == RenderQuality::Balanced);
    assert(governor.update(8.0, 0.0) == RenderQuality::Balanced);
    assert(governor.valid_samples() == 0);

    // Sustained overload steps down one tier, with hysteresis.
    for (int i = 0; i < 2; ++i)
        assert(governor.update(17.0, 16.667) == RenderQuality::Balanced);
    assert(governor.update(17.0, 16.667) == RenderQuality::Eco);
    for (int i = 0; i < 20; ++i)
        assert(governor.update(17.0, 16.667) == RenderQuality::Eco);

    // A long stable headroom period is required to step quality back up.
    for (int i = 0; i < 4; ++i)
        assert(governor.update(5.0, 16.667) == RenderQuality::Eco);
    assert(governor.update(5.0, 16.667) == RenderQuality::Balanced);
    for (int i = 0; i < 5; ++i)
        governor.update(5.0, 16.667);
    assert(governor.quality() == RenderQuality::Quality);

    const auto eco = RenderBudgetController::profile(RenderQuality::Eco);
    const auto balanced = RenderBudgetController::profile(RenderQuality::Balanced);
    const auto quality = RenderBudgetController::profile(RenderQuality::Quality);
    assert(eco.sharpening_strength < balanced.sharpening_strength);
    assert(balanced.sharpening_strength < quality.sharpening_strength);
    assert(!eco.temporal_effects_allowed);
    assert(!balanced.temporal_effects_allowed);
    assert(quality.temporal_effects_allowed);


    // The PS4 runtime starts at Quality only as a ceiling for explicitly enabled
    // effects. Sustained measured overload moves it down and disables temporal work.
    governor.reset(RenderQuality::Quality);
    assert(governor.quality() == RenderQuality::Quality);
    assert(RenderBudgetController::profile(governor.quality()).temporal_effects_allowed);
    for (int i = 0; i < 2; ++i)
        assert(governor.update(17.0, 16.667) == RenderQuality::Quality);
    assert(governor.update(17.0, 16.667) == RenderQuality::Balanced);
    assert(!RenderBudgetController::profile(governor.quality()).temporal_effects_allowed);
    for (int i = 0; i < 3; ++i)
        governor.update(17.0, 16.667);
    assert(governor.quality() == RenderQuality::Eco);
    assert(RenderBudgetController::profile(governor.quality()).sharpening_strength == 0.08f);

    // Long-term headroom restores one tier at a time, never instantly.
    for (int i = 0; i < 4; ++i)
        assert(governor.update(5.0, 16.667) == RenderQuality::Eco);
    assert(governor.update(5.0, 16.667) == RenderQuality::Balanced);

    governor.reset();
    assert(governor.quality() == RenderQuality::Balanced);
    assert(governor.valid_samples() == 0);
    return 0;
}

#include "whats_next/frame_generation_policy.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace whats_next;

int main() {
    FrameGenerationPolicy policy;
    const FrameContract frame{640, 480, PixelFormat::RGBA8, 2, 1, 1, false, false};

    assert(policy.decide(false, frame, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::BypassDisabled);
    assert(policy.decide(true, frame, HistoryDecision::RejectNoPrevious, {2}) ==
           FrameGenerationDecision::PresentRealFirstFrame);
    assert(policy.decide(true, frame, HistoryDecision::Accept, {1}) ==
           FrameGenerationDecision::PresentRealCadenceMismatch);
    assert(policy.decide(true, frame, HistoryDecision::Accept, {0}) ==
           FrameGenerationDecision::PresentRealCadenceMismatch);
    assert(policy.decide(true, frame, HistoryDecision::RejectDiscontinuity, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);
    assert(policy.decide(true, frame, HistoryDecision::RejectResourceChanged, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);
    assert(policy.decide(true, frame, HistoryDecision::Accept,
                         {std::numeric_limits<std::uint32_t>::max()}) ==
           FrameGenerationDecision::PresentRealCadenceMismatch);
    assert(policy.decide(true, frame, HistoryDecision::RejectSessionChanged, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);
    assert(policy.decide(true, frame, HistoryDecision::RejectInvalidCurrent, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    auto zero_frame_id = frame;
    zero_frame_id.frame_id = 0;
    assert(policy.decide(true, zero_frame_id, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    assert(policy.decide(true, frame, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::GenerateIntermediate);

    auto invalid = frame;
    invalid.width = 0;
    assert(policy.decide(true, invalid, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    auto unknown_format = frame;
    unknown_format.format = PixelFormat::Unknown;
    assert(policy.decide(true, unknown_format, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    auto missing_session = frame;
    missing_session.game_session = 0;
    assert(policy.decide(true, missing_session, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);

    auto missing_resources = frame;
    missing_resources.resource_generation = 0;
    assert(policy.decide(true, missing_resources, HistoryDecision::Accept, {2}) ==
           FrameGenerationDecision::PresentRealHistoryRejected);
    return 0;
}

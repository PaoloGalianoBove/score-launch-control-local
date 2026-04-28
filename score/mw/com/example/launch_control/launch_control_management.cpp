#include "score/mw/com/example/launch_control/launch_control_management.h"

namespace score::mw::com::example::launch_control
{

LaunchControlManagement::LaunchControlManagement(std::initializer_list<LaunchControlPhase> phases) noexcept
    : phases_{phases.begin(), phases.end()}, index_{0U}, sequence_counter_{1U}
{
}

bool LaunchControlManagement::HasNextPhase() const noexcept
{
    return index_ < phases_.size();
}

LaunchControlMessage LaunchControlManagement::NextMessage() noexcept
{
    LaunchControlMessage msg{};
    if (!HasNextPhase())
    {
        // Se non ci sono più fasi, ritorna una message con sequence_number=0 (utente può decidere come gestire).
        msg.sequence_number = 0U;
        msg.phase = LaunchControlPhase::Idle;
        return msg;
    }

    msg.sequence_number = sequence_counter_++;
    msg.phase = phases_[index_++];
    return msg;
}

}  // namespace score::mw::com::example::launch_control

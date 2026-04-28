#include "launch_control_message.h"

namespace score::mw::com::example::launch_control
{

std::string_view ToString(LaunchControlPhase phase) noexcept
{
    switch (phase)
    {
        case LaunchControlPhase::Idle:
            return "Idle - ship standing by";
        case LaunchControlPhase::Armed:
            return "Armed - systems aligned for hyperspace";
        case LaunchControlPhase::Preload:
            return "Preload - hyperdrive spooling up";
        case LaunchControlPhase::Launch:
            return "Launch - entering hyperspace";
        case LaunchControlPhase::Complete:
            return "Complete - hyperspace jump successful";
        default:
            return "Unknown phase - navigation status uncertain";
    }
}

}  // namespace score::mw::com::example::launch_control

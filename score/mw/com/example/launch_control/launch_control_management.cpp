#include "score/mw/com/example/launch_control/launch_control_management.h"

namespace score::mw::com::example::launch_control {

LaunchControlMessage LaunchControlManagement::MakeMessage(std::uint32_t seq) {
  LaunchControlMessage msg{};
  msg.sequence_number = seq;
  msg.phase = (seq < 5U) ? LaunchPhase::kPrelaunch : LaunchPhase::kLiftoff;
  return msg;
}

}  // namespace score::mw::com::example::launch_control

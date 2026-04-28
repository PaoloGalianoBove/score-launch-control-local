#pragma once

#include "score/mw/com/example/launch_control/launch_control_message.h"

namespace score::mw::com::example::launch_control {

class LaunchControlManagement {
 public:
  static LaunchControlMessage MakeMessage(std::uint32_t seq);
};

}  // namespace score::mw::com::example::launch_control

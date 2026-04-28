#pragma once

#include "launch_control_message.h"
#include "score/mw/com/types.h"

namespace score::mw::com::example::launch_control
{

template <typename Trait>
class LaunchControlInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<LaunchControlMessage> launch_control_message_{*this, "launch_control_message"};
};

using LaunchControlProxy = AsProxy<LaunchControlInterface>;
using LaunchControlSkeleton = AsSkeleton<LaunchControlInterface>;

}  // namespace score::mw::com::example::launch_control

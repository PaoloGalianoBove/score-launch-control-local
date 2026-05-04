#pragma once

#include "ping_pong_message.h"
#include "score/mw/com/types.h"

namespace score::mw::com::example::launch_control
{

/// Single interface class reused for both the Ping and Pong services.
/// The skeleton side sends the event; the proxy side subscribes to it.
/// - Ping service: sender holds PingPongSkeleton, receiver holds PingPongProxy
/// - Pong service: receiver holds PingPongSkeleton, sender holds PingPongProxy
template <typename Trait>
class PingPongInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<PingPongMessage> event_{*this, "event"};
};

using PingPongProxy    = AsProxy<PingPongInterface>;
using PingPongSkeleton = AsSkeleton<PingPongInterface>;

}  // namespace score::mw::com::example::launch_control

#include "score/mw/com/example/launch_control/ping_pong_interface.h"
#include "score/mw/com/impl/rust/bridge_macros.h"

BEGIN_EXPORT_MW_COM_INTERFACE(mw_com_PingPong,
                              ::score::mw::com::example::launch_control::PingPongProxy,
                              ::score::mw::com::example::launch_control::PingPongSkeleton)
EXPORT_MW_COM_EVENT(mw_com_PingPong,
                    ::score::mw::com::example::launch_control::PingPongMessage,
                    event_)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(mw_com_PingPongMessage, ::score::mw::com::example::launch_control::PingPongMessage)

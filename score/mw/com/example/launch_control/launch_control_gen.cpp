#include "score/mw/com/example/launch_control/launch_control_interface.h"
#include "score/mw/com/impl/rust/bridge_macros.h"

BEGIN_EXPORT_MW_COM_INTERFACE(mw_com_LaunchControl,
                              ::score::mw::com::example::launch_control::LaunchControlProxy,
                              ::score::mw::com::example::launch_control::LaunchControlSkeleton)
EXPORT_MW_COM_EVENT(mw_com_LaunchControl,
                    ::score::mw::com::example::launch_control::LaunchControlMessage,
                    launch_control_message_)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(mw_com_LaunchControlMessage, ::score::mw::com::example::launch_control::LaunchControlMessage)

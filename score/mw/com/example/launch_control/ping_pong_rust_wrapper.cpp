#include "score/mw/com/example/launch_control/ping_pong_message.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/rust/proxy_bridge.h"

extern "C" {

void mw_com_impl_call_dyn_ref_fnmut_sample_mw_com_PingPongMessage(
    const ::score::mw::com::impl::rust::FatPtr* /*boxed_fnmut*/,
    ::score::mw::com::impl::SamplePtr<::score::mw::com::example::launch_control::PingPongMessage>* placement_sample)
{
    if (placement_sample == nullptr)
    {
        return;
    }
    placement_sample->~SamplePtr<::score::mw::com::example::launch_control::PingPongMessage>();
}

}  // extern "C"

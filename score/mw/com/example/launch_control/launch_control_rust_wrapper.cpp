#include "score/mw/com/example/launch_control/launch_control_message.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/rust/proxy_bridge.h"

extern "C" {

void mw_com_impl_call_dyn_ref_fnmut_sample_mw_com_LaunchControlMessage(
    const ::score::mw::com::impl::rust::FatPtr* /*boxed_fnmut*/,
    ::score::mw::com::impl::SamplePtr<::score::mw::com::example::launch_control::LaunchControlMessage>* placement_sample)
{
    if (placement_sample == nullptr)
    {
        return;
    }
    // The invocation contract from the generated code places a SamplePtr<T>
    // into placement storage via placement-new. We must call its destructor
    // to correctly release contained resources and avoid leaks. We don't
    // attempt to call into Rust here (pure C++ path).
    placement_sample->~SamplePtr<::score::mw::com::example::launch_control::LaunchControlMessage>();
}

} // extern "C"

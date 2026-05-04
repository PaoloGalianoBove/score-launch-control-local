#pragma once

#include <cstdint>

namespace score::mw::com::example::launch_control
{

struct PingPongMessage
{
    std::uint32_t sequence_number{0U};
};

}  // namespace score::mw::com::example::launch_control

#include <cstdint>
#include <string_view>
namespace score::mw::com::example::launch_control
{

enum class LaunchControlPhase : std::uint8_t
{
    Idle = 0,
    Armed = 1,
    Preload = 2,
    Launch = 3,
    Complete = 4,
};

struct LaunchControlMessage
{
    std::uint32_t sequence_number{0U};
    LaunchControlPhase phase{LaunchControlPhase::Idle};
};

std::string_view ToString(LaunchControlPhase phase) noexcept;

}  // namespace score::mw::com::example::launch_control

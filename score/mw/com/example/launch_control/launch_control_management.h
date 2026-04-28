#pragma once

#include "score/mw/com/example/launch_control/launch_control_message.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace score::mw::com::example::launch_control
{

class LaunchControlManagement
{
  public:
    // Costruisce il manager con la sequenza di fasi che deve inviare.
    LaunchControlManagement(std::initializer_list<LaunchControlPhase> phases) noexcept;

    // True se ci sono ancora fasi non inviate.
    bool HasNextPhase() const noexcept;

    // Restituisce il prossimo LaunchControlMessage (con sequence_number incrementato).
    // Usare HasNextPhase() prima di chiamare NextMessage().
    LaunchControlMessage NextMessage() noexcept;

  private:
    std::vector<LaunchControlPhase> phases_;
    std::size_t index_{0U};
    std::uint32_t sequence_counter_{1U};
};

}  // namespace score::mw::com::example::launch_control

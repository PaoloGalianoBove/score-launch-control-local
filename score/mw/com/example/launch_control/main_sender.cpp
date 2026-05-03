#include "launch_control_interface.h"
#include "launch_control_management.h"

#include "score/mw/com/runtime.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

namespace lc = score::mw::com::example::launch_control;

int main(int argc, const char** argv)
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    const std::size_t num_cycles = (argc > 1) ? static_cast<std::size_t>(std::strtoul(argv[1], nullptr, 10)) : 5U;
    const auto cycle_time = std::chrono::milliseconds{100};

    // Create InstanceSpecifier via API pubblica
    auto spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/LaunchControl"});
    if (!spec_res.has_value())
    {
        std::cerr << "Invalid instance specifier: " << spec_res.error() << '\n';
        return EXIT_FAILURE;
    }
    const auto instance_specifier = spec_res.value();

    // Costruiamo il manager con la sequenza di fasi desiderata
    lc::LaunchControlManagement management{{
        lc::LaunchControlPhase::Idle,
        lc::LaunchControlPhase::Armed,
        lc::LaunchControlPhase::Preload,
        lc::LaunchControlPhase::Launch,
        lc::LaunchControlPhase::Complete}};

    // Create skeleton (usa l'API generata AsSkeleton)
    auto create_result = lc::LaunchControlSkeleton::Create(instance_specifier);
    if (!create_result.has_value())
    {
        std::cerr << "Unable to construct skeleton: " << create_result.error() << '\n';
        return EXIT_FAILURE;
    }
    auto& skeleton = create_result.value();

    const auto offer_result = skeleton.OfferService();
    if (!offer_result.has_value())
    {
        std::cerr << "Unable to offer service: " << offer_result.error() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "[Sender] Starting launch control sender...\n";

    for (std::size_t i = 0U; i < num_cycles; ++i)
    {
        if (!management.HasNextPhase())
        {
            std::cout << "[Sender] No more phases to send.\n";
            break;
        }

        const lc::LaunchControlMessage msg = management.NextMessage();

        auto sample_result = skeleton.launch_control_message_.Allocate();
        if (!sample_result.has_value())
        {
            std::cerr << "Failed to allocate sample: " << sample_result.error() << '\n';
            skeleton.StopOfferService();
            return EXIT_FAILURE;
        }
        auto sample = std::move(sample_result).value();

        sample->sequence_number = msg.sequence_number;
        sample->phase = msg.phase;

        const auto send_result = skeleton.launch_control_message_.Send(std::move(sample));
        if (!send_result.has_value())
        {
            std::cerr << "Failed to send sample: " << send_result.error() << '\n';
            skeleton.StopOfferService();
            return EXIT_FAILURE;
        }

        std::cout << "[Sender] seq=" << msg.sequence_number
                  << " phase=" << lc::ToString(msg.phase) << '\n';

        std::this_thread::sleep_for(cycle_time);
    }

    skeleton.StopOfferService();
    std::cout << "[Sender] Sender completed.\n";

    return EXIT_SUCCESS;
}

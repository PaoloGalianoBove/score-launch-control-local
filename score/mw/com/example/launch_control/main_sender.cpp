#include "launch_control_interface.h"
#include "launch_control_management.h"
#include "ping_pong_interface.h"

#include "score/mw/com/runtime.h"

#include "score/mw/com/runtime.h"
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace lc = score::mw::com::example::launch_control;

namespace
{
/// Returns the first positional command-line argument (i.e. the first arg that does not start
/// with '-') parsed as a size_t, or \p default_value when no such argument is found.
std::size_t ParseFirstPositionalArg(const int argc, const char** argv, const std::size_t default_value)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] != nullptr && argv[i][0] != '\0' && argv[i][0] != '-')
        {
            return static_cast<std::size_t>(std::strtoul(argv[i], nullptr, 10));
        }
    }
    return default_value;
}
}  // namespace

int main(int argc, const char** argv)
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    const std::size_t num_cycles = ParseFirstPositionalArg(argc, argv, 5U);
    const auto cycle_time = std::chrono::milliseconds{100};

    // --- Phase 1: Launch Control message sending ---

    auto spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/LaunchControl"});
    if (!spec_res.has_value())
    {
        std::cerr << "Invalid instance specifier: " << spec_res.error() << '\n';
        return EXIT_FAILURE;
    }
    const auto instance_specifier = spec_res.value();

    lc::LaunchControlManagement management{{
        lc::LaunchControlPhase::Idle,
        lc::LaunchControlPhase::Armed,
        lc::LaunchControlPhase::Preload,
        lc::LaunchControlPhase::Launch,
        lc::LaunchControlPhase::Complete}};

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

    std::this_thread::sleep_for(std::chrono::seconds(1));
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

        std::cout << "[Sender] seq=" << msg.sequence_number << " phase=" << lc::ToString(msg.phase) << '\n';

        std::this_thread::sleep_for(cycle_time);
    }

    skeleton.StopOfferService();
    std::cout << "[Sender] Launch control phase complete.\n";

    // --- Phase 2: Ping-Pong latency measurement ---

    std::cout << "[Sender] Starting ping-pong latency test...\n";

    // Create PingSkeleton and offer the Ping service (receiver subscribes to it)
    auto ping_spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/PingService"});
    if (!ping_spec_res.has_value())
    {
        std::cerr << "[Sender] Invalid ping specifier: " << ping_spec_res.error() << '\n';
        return EXIT_FAILURE;
    }
    auto ping_skeleton_res = lc::PingPongSkeleton::Create(ping_spec_res.value());
    if (!ping_skeleton_res.has_value())
    {
        std::cerr << "[Sender] Unable to construct ping skeleton: " << ping_skeleton_res.error() << '\n';
        return EXIT_FAILURE;
    }
    auto& ping_skeleton = ping_skeleton_res.value();

    if (!ping_skeleton.OfferService().has_value())
    {
        std::cerr << "[Sender] Unable to offer ping service\n";
        return EXIT_FAILURE;
    }

    // Find the PongService offered by the receiver
    auto pong_spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/PongService"});
    if (!pong_spec_res.has_value())
    {
        std::cerr << "[Sender] Invalid pong specifier: " << pong_spec_res.error() << '\n';
        ping_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }

    std::vector<score::mw::com::HandleType> pong_handles;
    std::cout << "[Sender] Waiting for pong service...\n";
    while (true)
    {
        auto find_res = lc::PingPongProxy::FindService(pong_spec_res.value());
        if (find_res.has_value() && !find_res.value().empty())
        {
            pong_handles = find_res.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto pong_proxy_res = lc::PingPongProxy::Create(pong_handles[0]);
    if (!pong_proxy_res.has_value())
    {
        std::cerr << "[Sender] Failed to create pong proxy: " << pong_proxy_res.error() << '\n';
        ping_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }
    auto pong_proxy = std::move(pong_proxy_res.value());

    if (!pong_proxy.event_.Subscribe(10).has_value())
    {
        std::cerr << "[Sender] Failed to subscribe to pong event\n";
        ping_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }

    // Ping-pong loop: send a ping, wait for the matching pong, measure RTT
    constexpr std::size_t kNumPings = 10000U;
    std::uint64_t total_rtt_us = 0U;

    std::ofstream rtt_log("rtt_log.txt");
    if (!rtt_log.is_open())
    {
        std::cerr << "[Sender] Failed to open rtt_log.txt for writing; RTT logging will be skipped\n";
    }

    for (std::uint32_t seq = 0U; seq < static_cast<std::uint32_t>(kNumPings); ++seq)
    {
        auto alloc_res = ping_skeleton.event_.Allocate();
        if (!alloc_res.has_value())
        {
            std::cerr << "[Sender] Failed to allocate ping sample\n";
            break;
        }
        auto ping_sample = std::move(alloc_res).value();
        ping_sample->sequence_number = seq;

        const auto t0 = std::chrono::steady_clock::now();

        if (!ping_skeleton.event_.Send(std::move(ping_sample)).has_value())
        {
            std::cerr << "[Sender] Failed to send ping " << seq << '\n';
            break;
        }
        std::cout << "[Sender] Ping seq=" << seq << " sent\n";

        // Wait for the pong with matching sequence number
        bool pong_received = false;
        while (!pong_received)
        {
            auto avail_res = pong_proxy.event_.GetNumNewSamplesAvailable();
            if (!avail_res.has_value() || avail_res.value() == 0U)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            auto get_res = pong_proxy.event_.GetNewSamples(
                [seq, &pong_received](score::mw::com::SamplePtr<lc::PingPongMessage> sample) {
                    if (sample && sample->sequence_number == seq)
                    {
                        pong_received = true;
                    }
                },
                avail_res.value());

            if (!get_res.has_value())
            {
                std::cerr << "[Sender] GetNewSamples (pong) failed\n";
                break;
            }
        }

        const auto rtt_us =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        total_rtt_us += static_cast<std::uint64_t>(rtt_us);
        std::cout << "[Sender] Pong seq=" << seq << " RTT=" << rtt_us << " us\n";

        if (rtt_log.is_open())
        {
            rtt_log << (rtt_us / 1000) << '\n';
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[Sender] Average RTT: " << (total_rtt_us / kNumPings) << " us\n";

    pong_proxy.event_.Unsubscribe();
    ping_skeleton.StopOfferService();
    std::cout << "[Sender] Sender completed.\n";

    return EXIT_SUCCESS;
}


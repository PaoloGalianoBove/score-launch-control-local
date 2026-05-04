#include "launch_control_interface.h"
#include "ping_pong_interface.h"

#include "score/mw/com/runtime.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace lc = score::mw::com::example::launch_control;

namespace
{
/// Returns the Nth (1-based) positional command-line argument (i.e. arguments that do not start
/// with '-') parsed as a size_t, or \p default_value when fewer than N positional arguments are
/// found. 1-based indexing is used so callers can naturally request the "1st", "2nd", etc. arg.
std::size_t ParseNthPositionalArg(const int argc, const char** argv, const int n, const std::size_t default_value)
{
    int positional_count = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] != nullptr && argv[i][0] != '\0' && argv[i][0] != '-')
        {
            ++positional_count;
            if (positional_count == n)
            {
                return static_cast<std::size_t>(std::strtoul(argv[i], nullptr, 10));
            }
        }
    }
    return default_value;
}
}  // namespace

int main(int argc, const char** argv)
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    // Numero massimo di cicli di ricezione (0 = infinito)
    const std::size_t max_cycles = ParseNthPositionalArg(argc, argv, 1, 0U);
    // Numero atteso di messaggi da ricevere prima di chiudere (0 = non usare)
    const std::size_t expected_messages = ParseNthPositionalArg(argc, argv, 2, 0U);

    // --- Phase 1: Launch Control message receiving ---

    auto spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/LaunchControl"});
    if (!spec_res.has_value())
    {
        std::cerr << "[Receiver] Invalid instance specifier: " << spec_res.error() << std::endl;
        return EXIT_FAILURE;
    }
    const auto instance_specifier = spec_res.value();

    std::cout << "[Receiver] Looking for service instance: " << instance_specifier.ToString() << std::endl;

    // Discovery loop: prova fino a che non trova almeno un'istanza offerta
    std::vector<score::mw::com::HandleType> handles;
    while (true)
    {
        auto find_res = lc::LaunchControlProxy::FindService(instance_specifier);
        if (!find_res.has_value())
        {
            std::cerr << "[Receiver] FindService returned error, retrying...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        auto found_handles = find_res.value();
        if (!found_handles.empty())
        {
            handles = found_handles;
            break;
        }
        std::cout << "[Receiver] No service found, retrying...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Crea la proxy usando il primo handle disponibile
    auto proxy_res = lc::LaunchControlProxy::Create(handles[0]);
    if (!proxy_res.has_value())
    {
        std::cerr << "[Receiver] Failed to create proxy: " << proxy_res.error() << std::endl;
        return EXIT_FAILURE;
    }
    auto proxy = std::move(proxy_res.value());

    // Subscribe all'evento (buffer_size = 10)
    const auto sub_res = proxy.launch_control_message_.Subscribe(10);
    if (!sub_res.has_value())
    {
        std::cerr << "[Receiver] Subscribe failed: " << sub_res.error() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "[Receiver] Subscribed to launch_control_message (buffer 10)\n";

    // Loop di ricezione: usiamo GetNumNewSamplesAvailable() e poi GetNewSamples() per drenare
    std::size_t cycles = 0U;
    std::size_t total_received = 0U;
    auto last_received_time = std::chrono::steady_clock::time_point{};
    constexpr auto kInactivityTimeout = std::chrono::milliseconds(1000);

    for (;;)
    {
        // Check how many new samples are available and drain exactly that many (capped)
        auto avail_res = proxy.launch_control_message_.GetNumNewSamplesAvailable();
        if (!avail_res.has_value())
        {
            std::cerr << "[Receiver] GetNumNewSamplesAvailable failed: " << avail_res.error() << std::endl;
            return EXIT_FAILURE;
        }

        const std::size_t available = avail_res.value();
        if (available == 0U)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else
        {
            const std::size_t to_drain = available; // drain everything the API reports

            // Collect samples into a temporary buffer so we can sort by sequence_number
            struct RecEntry
            {
                std::uint64_t seq;
                const void* ptr;
                std::string phase_str;
            };
            std::vector<RecEntry> buffer;
            buffer.reserve(to_drain);

            auto get_res = proxy.launch_control_message_.GetNewSamples(
                [&buffer](score::mw::com::SamplePtr<lc::LaunchControlMessage> sample) {
                    if (sample)
                    {
                        buffer.push_back(RecEntry{sample->sequence_number, static_cast<const void*>(sample.get()),
                                                   std::string(lc::ToString(sample->phase))});
                    }
                },
                to_drain);

            if (!get_res.has_value())
            {
                std::cerr << "[Receiver] GetNewSamples failed: " << get_res.error() << std::endl;
                return EXIT_FAILURE;
            }

            const std::size_t received_count = get_res.value();
            if (received_count > 0U)
            {
                // Sort by sequence number to present them in sending order
                std::sort(buffer.begin(), buffer.end(), [](const RecEntry& a, const RecEntry& b) {
                    return a.seq < b.seq;
                });

                for (const auto& e : buffer)
                {
                    std::cout << "[Receiver] ptr=" << e.ptr << " seq=" << e.seq << " phase=" << e.phase_str
                              << std::endl;
                }

                total_received += received_count;
                last_received_time = std::chrono::steady_clock::now();
            }
        }

        if (max_cycles != 0U)
        {
            ++cycles;
            if (cycles >= max_cycles)
            {
                std::cout << "[Receiver] reached max cycles (" << max_cycles << "), exiting.\n";
                break;
            }
        }

        // Exit if we've received the expected number of messages (if provided)
        if (expected_messages != 0U && total_received >= expected_messages)
        {
            std::cout << "[Receiver] received expected " << expected_messages << " messages, exiting." << std::endl;
            break;
        }

        // If we've received at least one message and then experienced inactivity, assume sender finished
        if (total_received > 0U && last_received_time != std::chrono::steady_clock::time_point{})
        {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_received_time > kInactivityTimeout)
            {
                std::cout << "[Receiver] no new messages for " << kInactivityTimeout.count()
                          << "ms, assuming sender finished, exiting." << std::endl;
                break;
            }
        }
    }

    proxy.launch_control_message_.Unsubscribe();

    // --- Phase 2: Ping-Pong ---

    std::cout << "[Receiver] Starting ping-pong phase...\n";

    // Offer the Pong service (sender subscribes to it)
    auto pong_spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/PongService"});
    if (!pong_spec_res.has_value())
    {
        std::cerr << "[Receiver] Invalid pong specifier: " << pong_spec_res.error() << '\n';
        return EXIT_FAILURE;
    }
    auto pong_skeleton_res = lc::PingPongSkeleton::Create(pong_spec_res.value());
    if (!pong_skeleton_res.has_value())
    {
        std::cerr << "[Receiver] Unable to construct pong skeleton: " << pong_skeleton_res.error() << '\n';
        return EXIT_FAILURE;
    }
    auto& pong_skeleton = pong_skeleton_res.value();

    if (!pong_skeleton.OfferService().has_value())
    {
        std::cerr << "[Receiver] Unable to offer pong service\n";
        return EXIT_FAILURE;
    }
    std::cout << "[Receiver] Pong service offered\n";

    // Find the Ping service offered by the sender
    auto ping_spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/PingService"});
    if (!ping_spec_res.has_value())
    {
        std::cerr << "[Receiver] Invalid ping specifier: " << ping_spec_res.error() << '\n';
        pong_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }

    std::vector<score::mw::com::HandleType> ping_handles;
    std::cout << "[Receiver] Waiting for ping service...\n";
    while (true)
    {
        auto find_res = lc::PingPongProxy::FindService(ping_spec_res.value());
        if (find_res.has_value() && !find_res.value().empty())
        {
            ping_handles = find_res.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto ping_proxy_res = lc::PingPongProxy::Create(ping_handles[0]);
    if (!ping_proxy_res.has_value())
    {
        std::cerr << "[Receiver] Failed to create ping proxy: " << ping_proxy_res.error() << '\n';
        pong_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }
    auto ping_proxy = std::move(ping_proxy_res.value());

    if (!ping_proxy.event_.Subscribe(10).has_value())
    {
        std::cerr << "[Receiver] Failed to subscribe to ping event\n";
        pong_skeleton.StopOfferService();
        return EXIT_FAILURE;
    }
    std::cout << "[Receiver] Subscribed to ping event\n";

    // Reflect each incoming ping back as a pong with the same sequence number
    constexpr auto kPingPongTimeout = std::chrono::milliseconds(5000);
    auto last_ping_time = std::chrono::steady_clock::now();

    for (;;)
    {
        auto avail_res = ping_proxy.event_.GetNumNewSamplesAvailable();
        if (!avail_res.has_value() || avail_res.value() == 0U)
        {
            if (std::chrono::steady_clock::now() - last_ping_time > kPingPongTimeout)
            {
                std::cout << "[Receiver] No ping received for " << kPingPongTimeout.count()
                          << "ms, exiting ping-pong phase.\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        auto get_res = ping_proxy.event_.GetNewSamples(
            [&pong_skeleton](score::mw::com::SamplePtr<lc::PingPongMessage> sample) {
                if (!sample)
                {
                    return;
                }
                const std::uint32_t seq = sample->sequence_number;
                std::cout << "[Receiver] Ping seq=" << seq << " received, sending pong\n";

                auto alloc_res = pong_skeleton.event_.Allocate();
                if (!alloc_res.has_value())
                {
                    std::cerr << "[Receiver] Failed to allocate pong sample\n";
                    return;
                }
                auto pong_sample = std::move(alloc_res).value();
                pong_sample->sequence_number = seq;

                if (!pong_skeleton.event_.Send(std::move(pong_sample)).has_value())
                {
                    std::cerr << "[Receiver] Failed to send pong seq=" << seq << '\n';
                }
            },
            avail_res.value());

        if (!get_res.has_value())
        {
            std::cerr << "[Receiver] GetNewSamples (ping) failed\n";
            break;
        }

        if (get_res.value() > 0U)
        {
            last_ping_time = std::chrono::steady_clock::now();
        }
    }

    ping_proxy.event_.Unsubscribe();
    pong_skeleton.StopOfferService();
    std::cout << "[Receiver] Receiver completed.\n";

    return EXIT_SUCCESS;
}


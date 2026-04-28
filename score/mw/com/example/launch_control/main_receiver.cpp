#include "launch_control_interface.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace lc = score::mw::com::example::launch_control;

int main(int argc, char** argv)
{
    // Numero massimo di cicli di ricezione (0 = infinito)
    const std::size_t max_cycles = (argc > 1) ? static_cast<std::size_t>(std::strtoul(argv[1], nullptr, 10)) : 0U;

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
            std::cerr << "[Receiver] FindService returned error, retrying in 1s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        auto found_handles = find_res.value();
        if (!found_handles.empty())
        {
            handles = found_handles;
            break;
        }
        std::cout << "[Receiver] No service found, retrying in 1s...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
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

    // Loop di ricezione: chiamiamo GetNewSamples ripetutamente con callback
    std::size_t cycles = 0U;
    for (;;)
    {
        auto get_res = proxy.launch_control_message_.GetNewSamples(
            [](score::mw::com::SamplePtr<lc::LaunchControlMessage> sample) {
                if (sample)
                {
                    std::cout << "[Receiver] seq=" << sample->sequence_number
                              << " phase=" << lc::ToString(sample->phase) << std::endl;
                }
            },
            1U);

        if (!get_res.has_value())
        {
            std::cerr << "[Receiver] GetNewSamples failed: " << get_res.error() << std::endl;
            return EXIT_FAILURE;
        }

        const std::size_t received_count = get_res.value();
        if (received_count == 0U)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
    }

    proxy.launch_control_message_.Unsubscribe();
    return EXIT_SUCCESS;
}

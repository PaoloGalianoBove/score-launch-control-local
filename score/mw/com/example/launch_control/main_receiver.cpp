#include "launch_control_interface.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "score/mw/com/runtime.h"

namespace lc = score::mw::com::example::launch_control;

auto main(int argc, const char** argv) -> int
{
    score::mw::com::runtime::InitializeRuntime(argc, argv);
    // Numero massimo di cicli di ricezione (0 = infinito)
    const std::size_t max_cycles = (argc > 1) ? static_cast<std::size_t>(std::strtoul(argv[1], nullptr, 10)) : 0U;
    // Numero atteso di messaggi da ricevere prima di chiudere (0 = non usare)
    const std::size_t expected_messages = (argc > 2) ? static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10)) : 0U;
    // Tempo di attesa tra i tentativi di discovery se il servizio non è ancora disponibile
    const int ms_wait_for_service_aval = 500;
    // Tempo di attesa tra i tentativi di lettura dei messaggi se il buffer è vuoto (per evitare busy wait)
    const int ms_wait_for_messages_in_buffer = 250;
    //Creo l'istanceSpecifier per la ricerca del servizio (usando l'API pubblica) specificata in mw_com_config.json
    auto spec_res = score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/LaunchControl"});
    
    //Verifico che l'instance specifier sia valido, altrimenti esco con errore (non ha senso continuare se non possiamo identificare il servizio da cercare)
    if (!spec_res.has_value())
    {
        std::cerr << "[Receiver] Invalid instance specifier: " << spec_res.error() << '\n';
        return EXIT_FAILURE;
    }


    const auto& instance_specifier = spec_res.value();

    std::cout << "[Receiver] Looking for service instance: " << instance_specifier.ToString() << '\n';

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
        const auto& found_handles = find_res.value();
        if (!found_handles.empty())
        {
            handles = found_handles;
            break;
        }
        
        std::cout << "[Receiver] No service found, retrying in " << ms_wait_for_service_aval << " ms...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(ms_wait_for_service_aval)); //Controllo ogni mezzo secondo se il servizio è disponibile, per non uccidere la CPU
    }

    // Crea la proxy usando il primo handle disponibile
    auto proxy_res = lc::LaunchControlProxy::Create(handles[0]);
    if (!proxy_res.has_value())
    {
        std::cerr << "[Receiver] Failed to create proxy: " << proxy_res.error() << '\n';
        return EXIT_FAILURE;
    }
    auto proxy = std::move(proxy_res.value());

    // Subscribe all'evento (buffer_size = 10)
    const auto sub_res = proxy.launch_control_message_.Subscribe(10);
    if (!sub_res.has_value())
    {
        std::cerr << "[Receiver] Subscribe failed: " << sub_res.error() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "[Receiver] Subscribed to launch_control_message (buffer 10)\n";

    // Loop di ricezione: usiamo GetNumNewSamplesAvailable() e poi GetNewSamples() per drenare
    std::size_t cycles = 0U;
    std::size_t total_received = 0U;
    auto last_received_time = std::chrono::steady_clock::time_point{};
    constexpr auto kInactivityTimeout = std::chrono::milliseconds(500);

    for (;;)
    {
        // Check how many new samples are available and drain exactly that many (capped)
        auto avail_res = proxy.launch_control_message_.GetNumNewSamplesAvailable();
        if (!avail_res.has_value())
        {
            std::cerr << "[Receiver] GetNumNewSamplesAvailable failed: " << avail_res.error() << '\n';
            return EXIT_FAILURE;
        }

        const std::size_t available = avail_res.value();
        if (available == 0U)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms_wait_for_messages_in_buffer)); //In pratica sto cercando di non uccidere la CPU e leggo i messaggi NEL BUFFER (quindi dal mw) ogni 250ms, se non ci sono messaggi aspetto e riprovo
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
                std::cerr << "[Receiver] GetNewSamples failed: " << get_res.error() << '\n';
                return EXIT_FAILURE;
            }

            const std::size_t received_count = get_res.value();
            if (received_count > 0U)
            {
                // Sort by sequence number to present them in sending order
                std::sort(buffer.begin(), buffer.end(), [](const RecEntry& previous, const RecEntry& sequent) {
                    return previous.seq < sequent.seq;
                });

                for (const auto& elements : buffer)
                {
                    std::cout << "[Receiver] ptr=" << elements.ptr << " seq=" << elements.seq << " phase=" << elements.phase_str
                              << '\n';
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
            std::cout << "[Receiver] received expected " << expected_messages << " messages, exiting." << '\n';
            break;
        }

        // If we've received at least one message and then experienced inactivity, assume sender finished
        if (total_received > 0U && last_received_time != std::chrono::steady_clock::time_point{})
        {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_received_time > kInactivityTimeout)
            {
                std::cout << "[Receiver] no new messages for " << kInactivityTimeout.count()
                          << "ms, assuming sender finished, exiting." << '\n';
                break;
            }
        }
    }

    proxy.launch_control_message_.Unsubscribe();
    return EXIT_SUCCESS;
}

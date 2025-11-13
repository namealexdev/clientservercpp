#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <libinclude/netlib.h>

template <typename FactoryMode, int server_workers = 0>
void test_speed_concurrency(uint16_t port, int clients, int test_duration_sec = 60)
{
    std::cout << "START multiclient concurrency TEST" << std::endl;
    std::cout << "Clients: " << clients << ", Duration: " << test_duration_sec << "s" << std::endl;

    std::atomic<bool> stop_flag{false};
    std::atomic<int> clients_connected{0};

    // Server thread
    std::thread server_thread([&]() {
        std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();
        ServerConfig srv_conf{ .port = port, .max_connections = clients };

        auto srv = factory->createServer(srv_conf);
        if (!srv || !srv->StartListen(server_workers)) {
            std::cerr << "Failed to start server" << std::endl;
            return;
        }

        std::cout << "Server started on port " << port << std::endl;

        // Add handler for new connections
        srv->AddHandlerEvent(EventType::ClientConnected, [&](void* data) {
            clients_connected++;
        });

        // Wait for test duration or stop signal
        auto start_time = std::chrono::steady_clock::now();
        while (!stop_flag) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            if (elapsed.count() >= test_duration_sec) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Stopping server..." << std::endl;
        srv->Stop();
    });

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Client threads
    std::vector<std::thread> client_threads;
    std::vector<std::unique_ptr<IClient>> client_objects;
    std::atomic<uint64_t> total_bytes_sent{0};
    std::atomic<uint64_t> total_packets_sent{0};

    for (int i = 0; i < clients; i++) {
        client_threads.emplace_back([&, client_id = i]() {
            std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();
            ClientConfig cli_conf{
                .server_ip = "127.0.0.1",
                .server_port = port
            };

            std::unique_ptr<IClient> cli = factory->createClient(cli_conf);
            if (!cli) {
                std::cerr << "Client " << client_id << " failed to create" << std::endl;
                return;
            }

            client_objects.emplace_back(std::move(cli));

            // Connect to server
            cli->Connect();

            // Wait for connection
            int connect_attempts = 0;
            while (cli->GetClientState() != "WAITING" && connect_attempts < 50) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                connect_attempts++;
            }

            if (cli->GetClientState() != "WAITING") {
                std::cerr << "Client " << client_id << " failed to connect, state: "
                          << cli->GetClientState() << std::endl;
                return;
            }

            // Test data - 1KB packet
            const int packet_size = 1024;
            std::vector<char> test_data(packet_size, 'X');

            // Send loop
            auto start_time = std::chrono::steady_clock::now();
            uint64_t local_bytes_sent = 0;
            uint64_t local_packets_sent = 0;

            while (!stop_flag) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
                if (elapsed.count() >= test_duration_sec) {
                    break;
                }

                try {
                    cli->SendToSocket(test_data.data(), packet_size);
                    local_bytes_sent += packet_size;
                    local_packets_sent++;

                    // Small delay to prevent overwhelming
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "Client " << client_id << " send error: " << e.what() << std::endl;
                    break;
                }
            }

            total_bytes_sent += local_bytes_sent;
            total_packets_sent += local_packets_sent;

            // Disconnect
            cli->Disconnect();
        });
    }

    // Statistics thread
    std::thread stats_thread([&]() {
        auto start_time = std::chrono::steady_clock::now();
        auto last_time = start_time;
        uint64_t last_bytes = 0;
        uint64_t last_packets = 0;

        while (!stop_flag) {
            auto now = std::chrono::steady_clock::now();
            auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);

            if (total_elapsed.count() >= test_duration_sec) {
                break;
            }

            // Print stats every second
            auto elapsed_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
            if (elapsed_since_last.count() >= 1000) {
                uint64_t current_bytes = total_bytes_sent.load();
                uint64_t current_packets = total_packets_sent.load();

                double interval_sec = elapsed_since_last.count() / 1000.0;
                double bytes_per_sec = (current_bytes - last_bytes) / interval_sec;
                double packets_per_sec = (current_packets - last_packets) / interval_sec;

                double mbps = (bytes_per_sec * 8) / (1024 * 1024);

                std::cout << "[" << std::setw(3) << total_elapsed.count() << "s] "
                          << "Rate: " << std::setw(8) << std::fixed << std::setprecision(2) << mbps << " Mbps, "
                          << std::setw(8) << std::fixed << std::setprecision(0) << packets_per_sec << " pkt/s, "
                          << "Connected: " << clients_connected.load() << "/" << clients
                          << std::endl;

                last_time = now;
                last_bytes = current_bytes;
                last_packets = current_packets;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Wait for test duration
    std::this_thread::sleep_for(std::chrono::seconds(test_duration_sec));
    stop_flag = true;

    // Wait for all threads
    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (stats_thread.joinable()) {
        stats_thread.join();
    }

    if (server_thread.joinable()) {
        server_thread.join();
    }

    // Final statistics
    auto total_bytes = total_bytes_sent.load();
    auto total_packets = total_packets_sent.load();
    double avg_mbps = (total_bytes * 8.0 / test_duration_sec) / (1024 * 1024);
    double avg_pps = total_packets / test_duration_sec;

    std::cout << "\n=== TEST COMPLETED ===" << std::endl;
    std::cout << "Total bytes sent: " << total_bytes << std::endl;
    std::cout << "Total packets sent: " << total_packets << std::endl;
    std::cout << "Average rate: " << std::fixed << std::setprecision(2) << avg_mbps << " Mbps" << std::endl;
    std::cout << "Average packets: " << std::fixed << std::setprecision(0) << avg_pps << " pkt/s" << std::endl;
    std::cout << "Clients connected: " << clients_connected.load() << "/" << clients << std::endl;
}

// Пример использования:
int main() {
    // Тест с однопоточным режимом
    std::cout << "=== SINGLETHREAD TEST ===" << std::endl;
    test_speed_concurrency<SinglethreadFactory>(12345, 5, 5);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Тест с многопоточным режимом
    std::cout << "\n=== MULTITHREAD TEST ===" << std::endl;
    test_speed_concurrency<MultithreadFactory>(12346, 10, 5);

    return 0;
}

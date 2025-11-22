#ifndef TESTS_H
#define TESTS_H


#include <libinclude/netlib.h>

#define BOOST_TEST_MODULE ServerClientTests
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <thread>
#include <chrono>

// Общие вспомогательные функции
namespace TestUtils {
bool WaitForCondition(std::function<bool()> condition, int timeout_ms = 5000) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}

template<typename ServerImpl>
struct ServerClientFixture {
    ServerClientFixture() {
        // Настройка конфигурации
        server_config_.port = 54321;

        client_config_.server_ip = "127.0.0.1";
        client_config_.server_port = 54321;
    }

    ~ServerClientFixture() {
        if (client) client->Stop();
        if (server) server->Stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void SetupServerAndClient() {
        server = std::make_unique<ServerImpl>(server_config_);
        // Здесь должен быть ваш клиент
        // client = std::make_unique<YourClientImpl>(client_config_);
    }

    bool WaitForConnection(int timeout_ms = 3000) {
        return TestUtils::WaitForCondition([this]() {
            return client->IsConnected();
        }, timeout_ms);
    }

    bool WaitForClientState(ClientState expected_state, int timeout_ms = 3000) {
        return TestUtils::WaitForCondition([this, expected_state]() {
            return client->ClientState() == expected_state;
        }, timeout_ms);
    }

    bool WaitForClientCount(int expected_count, int timeout_ms = 3000) {
        return TestUtils::WaitForCondition([this, expected_count]() {
            return server->CountClients() == expected_count;
        }, timeout_ms);
    }

    ServerConfig server_config_;
    ClientConfig client_config_;
    std::unique_ptr<ServerImpl> server;
    std::unique_ptr<IClient> client; // Замените на ваш клиент
};

#endif // TESTS_H

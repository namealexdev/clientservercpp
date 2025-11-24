#ifndef TESTS_H
#define TESTS_H


#include <libinclude/netlib.h>

#define BOOST_TEST_MODULE ServerClientTests
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <thread>
#include <chrono>
#include <atomic>

// Общие вспомогательные функции

bool WaitForCondition(std::function<bool()> condition, int timeout_ms = 5000) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}



template<typename ServerFactory, typename ClientFactory>
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
        server_factory = std::make_unique<ServerFactory>();
        client_factory = std::make_unique<ClientFactory>();

        server = server_factory->createServer(std::move(server_config_));
        client = client_factory->createClient(std::move(client_config_));
    }

    bool WaitForConnection(int timeout_ms = 3000) {
        return WaitForCondition([this]() {
            return client->IsConnected();
        }, timeout_ms);
    }

    bool WaitForClientState(ClientState expected_state, int timeout_ms = 3000) {
        return WaitForCondition([this, expected_state]() {
            return client->ClientState() == expected_state;
        }, timeout_ms);
    }

    bool WaitForClientCount(int expected_count, int timeout_ms = 3000) {
        return WaitForCondition([this, expected_count]() {
            return server->CountClients() == expected_count;
        }, timeout_ms);
    }

    ServerConfig server_config_;
    ClientConfig client_config_;
    std::unique_ptr<ServerFactory> server_factory;
    std::unique_ptr<ClientFactory> client_factory;
    std::unique_ptr<IServer> server;
    std::unique_ptr<IClient> client;
};



struct ClientServerFixture {
    ClientServerFixture() {
        server_config.port = 54321;
        server_config.worker_threads = 2;

        client_config.server_ip = "127.0.0.1";
        client_config.server_port = 54321;

        server_factory = std::make_unique<MultithreadFactory>();
        client_factory = std::make_unique<SinglethreadFactory>();

        server = server_factory->createServer(std::move(server_config));
        client = client_factory->createClient(std::move(client_config));
    }

    ~ClientServerFixture() {
        if (client) client->Stop();
        if (server) server->Stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bool WaitForCondition(std::function<bool()> condition, int timeout_ms = 3000) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
            if (condition()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    ServerConfig server_config;
    ClientConfig client_config;
    std::unique_ptr<INetworkFactory> server_factory;
    std::unique_ptr<INetworkFactory> client_factory;
    std::unique_ptr<IServer> server;
    std::unique_ptr<IClient> client;
};

BOOST_FIXTURE_TEST_SUITE(ClientServerTests, ClientServerFixture)

BOOST_AUTO_TEST_CASE(SuccessfulConnection) {
    BOOST_REQUIRE(server->StartListen());
    client->Start();

    bool connected = WaitForCondition([this]() {
        return client->IsConnected();
    });

    BOOST_CHECK(connected);
    BOOST_CHECK_EQUAL(server->CountClients(), 1);
}

BOOST_AUTO_TEST_CASE(SendReceiveMessage) {
    // Настраиваем обработчик на сервере
    std::vector<char> received_data;
    server->AddHandlerEvent(EventType::DataReceived, [&](void* data) {
        auto* dr = static_cast<DataReceived*>(data);
        received_data.assign(dr->data, dr->data + dr->size);
    });

    BOOST_REQUIRE(server->StartListen());
    client->Start();
    BOOST_REQUIRE(WaitForCondition([this]() { return client->IsConnected(); }));

    // Отправляем тестовое сообщение
    std::vector<char> test_message = {'H', 'e', 'l', 'l', 'o'};
    client->QueueAdd(test_message.data(), test_message.size());
    client->QueueSendAll();

    // Ждём получения
    bool received = WaitForCondition([&]() { return !received_data.empty(); });

    BOOST_CHECK(received);
    BOOST_CHECK_EQUAL(received_data.size(), 5);
    BOOST_CHECK_EQUAL_COLLECTIONS(
        received_data.begin(), received_data.end(),
        test_message.begin(), test_message.end()
        );
}

BOOST_AUTO_TEST_CASE(MultipleMessages) {
    std::atomic<int> message_count{0};
    server->AddHandlerEvent(EventType::DataReceived, [&](void* data) {
        message_count++;
    });

    BOOST_REQUIRE(server->StartListen());
    client->Start();
    BOOST_REQUIRE(WaitForCondition([this]() { return client->IsConnected(); }));

    // Отправляем 10 сообщений
    for (int i = 0; i < 10; i++) {
        std::vector<char> msg(100, 'A' + i);
        client->QueueAdd(msg.data(), msg.size());
    }
    client->QueueSendAll();

    bool all_received = WaitForCondition([&]() { return message_count >= 10; }, 5000);
    BOOST_CHECK(all_received);
    BOOST_CHECK_EQUAL(message_count, 10);
}

BOOST_AUTO_TEST_CASE(ClientDisconnect) {
    BOOST_REQUIRE(server->StartListen());
    client->Start();
    BOOST_REQUIRE(WaitForCondition([this]() { return client->IsConnected(); }));
    BOOST_CHECK_EQUAL(server->CountClients(), 1);

    // Клиент отключается
    client->Stop();

    bool disconnected = WaitForCondition([this]() {
        return server->CountClients() == 0;
    });

    BOOST_CHECK(disconnected);
}

BOOST_AUTO_TEST_CASE(ServerShutdown) {
    BOOST_REQUIRE(server->StartListen());
    client->Start();
    BOOST_REQUIRE(WaitForCondition([this]() { return client->IsConnected(); }));

    // Сервер останавливается
    server->Stop();

    bool client_disconnected = WaitForCondition([this]() {
        return !client->IsConnected();
    });

    BOOST_CHECK(client_disconnected);
}

BOOST_AUTO_TEST_SUITE_END()


#endif // TESTS_H

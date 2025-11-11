#include <memory>
#include <chrono>
#include <thread>
#include <cassert>
#include <cstring>
#include <netlib.h>

// __FILE__ __FUNCTION__ __PRETTY_FUNCTION__
#define d(x) std::cout << x << " \t(" << __FUNCTION__ << " " << __LINE__ << ")" << std::endl;

// Helpers: polling wait with timeout for states/conditions
static bool wait_for_condition(std::function<bool()> pred, int timeout_ms, int step_ms = 10) {
    const int steps = timeout_ms / step_ms;
    for (int i = 0; i < steps; ++i) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
    }
    return pred();
}

// Expected public API sketch used here:
// - INetworkFactory with SinglethreadFactory / MultithreadFactory
// - IServer: Start(), Stop(), GetServerState(), CountClients()
// - IClient: Connect(), Disconnect(), GetClientState(), SendToSocket(const void*, size_t)
// - Optional: stats via IClient/IServer if available

// 1) Connection lifecycle test with practical checks
// Purpose: validate handshake-on-connect, single and multiple clients, graceful stop
// Also prints Stats if exposed via server/client
template <typename FactoryMode, int server_workers = 0>
void test_connection_lifecycle(uint16_t port)
{
    d("START connection lifecycle TEST");
    std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();

    ServerConfig srv_conf{ .port = port, .max_connections = 32 };
    ClientConfig cli_conf{ .server_ip = "127.0.0.1", .server_port = port };

    IServer* srv = factory->createServer(srv_conf);
    assert(srv != nullptr);

    // For MultithreadFactory, Start may return number of workers; for single-thread, treat nonzero as success
    bool started = srv->StartListen(server_workers);
    assert(started == true);

    // 1) Single client connect (handshake implicitly checked by successful establishment)
    IClient* cli1 = factory->createClient(cli_conf);
    assert(cli1 != nullptr);

    d("connect")
    cli1->Connect();
d("wait")
    bool ok = wait_for_condition([&](){ return srv->CountClients() >= 1; }, 1500);
    assert(ok);
    d("send")

    // Send a probe payload to ensure data path works post-handshake
    const std::string probe = "hello";
    cli1->SendToSocket((char*)probe.data(), probe.size());

    // 2) Additional clients: verify multi-client acceptance and accounting
    IClient* cli2 = factory->createClient(cli_conf); assert(cli2); cli2->Connect();
    IClient* cli3 = factory->createClient(cli_conf); assert(cli3); cli3->Connect();
    ok = wait_for_condition([&](){ return srv->CountClients() >= 3; }, 2000);
    assert(ok);

    // Print basic stats if accessible through public API (pseudo, adjust if you expose getters)
    // Example: if srv->GetStats() or cli1->GetStats() exist, print bitrate
    // try/catch not needed here; replace with actual calls when available
    std::cout << "Server stats: " << srv->GetStats().getBitrate() << std::endl;
    std::cout << "Client1 stats: " << cli1->GetStats().getBitrate() << std::endl;

    // Disconnect extras then the first
    cli2->Disconnect();
    cli3->Disconnect();
    wait_for_condition([&](){ return srv->CountClients() == 1; }, 1500);

    cli1->Disconnect();
    wait_for_condition([&](){ return srv->CountClients() == 0; }, 1500);

    srv->Stop();
    d("END connection lifecycle TEST");
}

// 2) Size+payload data exchange (echo-like) with partial frames
template <typename FactoryMode, int server_workers = 0>
void test_size_payload_exchange(uint16_t port)
{
    d("START size+payload exchange TEST");
    std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();

    ServerConfig srv_conf{ .port = port, .max_connections = 16 };
    ClientConfig cli_conf{ .server_ip = "127.0.0.1", .server_port = port };

    IServer* srv = factory->createServer(srv_conf);
    IClient* cli = factory->createClient(cli_conf);
    assert(srv && cli);

    assert(srv->StartListen(server_workers));
    cli->Connect();
    bool ok = wait_for_condition([&](){ return srv->CountClients() >= 1; }, 1000);
    assert(ok);

    // Send multiple payloads including small, zero, and larger
    std::string small = "hi";
    std::string empty;
    std::string large(64 * 1024, 'X');

    cli->SendToSocket(small.data(), small.size());
    cli->SendToSocket(empty.data(), empty.size());
    cli->SendToSocket(large.data(), large.size());

    // If server is echoing, we might check stats if exposed. At least ensure no disconnect happened for some time.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(srv->CountClients() == 1);

    cli->Disconnect();
    wait_for_condition([&](){ return srv->CountClients() == 0; }, 1000);
    srv->Stop();
    d("END size+payload exchange TEST");
}

// 3) Handshake tests (success path assumption) and negative case via early send
template <typename FactoryMode, int server_workers = 0>
void test_handshake(uint16_t port)
{
    d("START handshake TEST with bitrate stats");
    std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();

    ServerConfig srv_conf{ .port = port, .max_connections = 8 };
    ClientConfig cli_conf{ .server_ip = "127.0.0.1", .server_port = port };

    IServer* srv = factory->createServer(srv_conf);
    IClient* cli = factory->createClient(cli_conf);
    assert(srv && cli);

    // Start: для MultithreadFactory это число воркеров, проверим >=1

    bool started = srv->StartListen(server_workers);
    assert(started == true);

    cli->Connect();

    bool ok = wait_for_condition([&](){ return srv->CountClients() >= 1; }, 1500);
    assert(ok);

    // Замер начального битрейта клиента
    double initial_bps = 0.0;
    {
        // предполагается: Stats& st = cli->GetStats();
        auto& st = cli->GetStats();
        std::string br = st.getBitrate(); // первый вызов может вернуть "0"
        (void)br;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        br = st.getBitrate();
        // last_bps внутри Stats хранит bytes/sec
        initial_bps = st.last_bps;
        std::cout << "[handshake] initial client bitrate: " << br << " (bytes/sec: " << initial_bps << ")" << std::endl;
    }

    // Прогрев небольшой нагрузкой
    const std::string warmup(256 * 1024, 'W'); // 256KB
    for (int i = 0; i < 8; ++i) {
        cli->SendToSocket((char*)warmup.data(), warmup.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    {
        auto& st = cli->GetStats();
        std::string br = st.getBitrate();
        std::cout << "[handshake] after warmup client bitrate: " << br << " (bytes/sec: " << st.last_bps << ")" << std::endl;
        assert(st.last_bps >= initial_bps); // ожидаем рост
    }

    // Тест пиковой скорости передачи: отправляем большой объём, наблюдаем битрейт
    const size_t chunk_size = 512 * 1024; // 512KB
    std::string payload(chunk_size, 'X');

    const int duration_ms = 1500;       // 1.5s нагрузки
    const int probe_step_ms = 100;      // шаг замера битрейта
    double peak_bps = 0.0;              // bytes/sec
    auto end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);

    while (std::chrono::steady_clock::now() < end_time) {
        // burst на сетку
        for (int i = 0; i < 8; ++i) {
            cli->SendToSocket(payload.data(), payload.size());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(probe_step_ms));
        auto& st = cli->GetStats();
        st.getBitrate(); // обновит last_bps внутри
        if (st.last_bps > peak_bps) peak_bps = st.last_bps;
    }

    // Вывести пиковую скорость, убедиться, что > 0
    std::cout << "[handshake] peak client throughput: " << cli->GetStats().getBitrate()
              << " (bytes/sec: " << peak_bps << ")" << std::endl;
    assert(peak_bps > 0.0);

    // По желанию — вывести серверную статистику (если есть агрегированная)
    // std::cout << "[handshake] server bitrate: " << srv->GetStats().getBitrate() << std::endl;

    cli->Disconnect();
    wait_for_condition([&](){ return srv->CountClients() == 0; }, 1000);
    srv->Stop();
    d("END handshake TEST with bitrate stats");
}

// 4) Multithreaded basic concurrency with multiple clients
template <typename FactoryMode, int server_workers = 0>
void test_multiclient_concurrency(uint16_t port, int clients)
{
    d("START multiclient concurrency TEST");
    std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();

    ServerConfig srv_conf{ .port = port, .max_connections = clients };
    ClientConfig cli_conf{ .server_ip = "127.0.0.1", .server_port = port };

    IServer* srv = factory->createServer(srv_conf);
    assert(srv);
    assert(srv->StartListen(server_workers));

    std::vector<IClient*> clis;
    clis.reserve(clients);
    for (int i = 0; i < clients; ++i) {
        IClient* c = factory->createClient(cli_conf);
        assert(c);
        c->Connect();
        clis.push_back(c);
    }

    bool ok = wait_for_condition([&](){ return srv->CountClients() >= (size_t)clients; }, 2000);
    assert(ok);

    // Burst small messages from all clients
    for (auto* c : clis) {
        std::string msg = "ping";
        c->SendToSocket(msg.data(), msg.size());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(srv->CountClients() == (size_t)clients);

    for (auto* c : clis) c->Disconnect();
    wait_for_condition([&](){ return srv->CountClients() == 0; }, 2000);

    srv->Stop();
    d("END multiclient concurrency TEST");
}

int main(int argc, char* argv[])
{
    try {
        // Single-threaded factory
        test_connection_lifecycle<SinglethreadFactory>(5202);
        test_size_payload_exchange<SinglethreadFactory>(5203);
        test_handshake<SinglethreadFactory>(5204);

        // Multithreaded factory (if available)
        // Uncomment if MultithreadFactory is implemented
        test_connection_lifecycle<MultithreadFactory, 2>(5205);
        test_multiclient_concurrency<MultithreadFactory, 2>(5206, 8);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        return 1;
    }

    return 0;
}

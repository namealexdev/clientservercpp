#include <memory>
#include <netlib.h>

// __FILE__ __FUNCTION__ __PRETTY_FUNCTION__
#define d(x) std::cout << x << " \t(" << __FUNCTION__ << " " << __LINE__ << ")" << std::endl;


template <typename FactoryMode, int count_ths = 0>
void test1_connection_state()
{
    d("START connection state TEST");
    std::unique_ptr<INetworkFactory> factory = std::make_unique<FactoryMode>();

    ServerConfig srv_conf{
        .port = 5202,
        .max_connections = 10,
    };
    ClientConfig cli_conf{
        .server_ip = "127.0.0.1",
        .server_port = 5202,
    };
    IServer* srv = factory->createServer(std::move(srv_conf));
    IClient* cli = factory->createClient(cli_conf);

    std::cout << "srv:" << srv->GetServerState() << " cli:" << cli->GetClientState() << std::endl;

    bool isstart = srv->Start();
    d("server start " << isstart)

    // srv->AddHandlerEvent(EventType::ClientConnect, [&](void* ){
    //     d("[server new client connected]");
    // });
    // srv->AddHandlerEvent(EventType::Disconnected, [&](void* ){
    //     d("[server new client DISSconnected]");
    // });

    // cli->setAutoSend(1);
    cli->Connect();

    std::cout << "[f after connect] srv:" << srv->GetServerState()
              << " (" << srv->CountClients() << " clis)"
              << " cli:" << cli->GetClientState() << std::endl;
    usleep(2*1000);

    std::cout << "[s after connect] srv:" << srv->GetServerState()
              << " (" << srv->CountClients() << " clis)"
              << " cli:" << cli->GetClientState() << std::endl;

    string message = "some data";
    cli->SendToSocket(message.data(), message.size());
    usleep(2*1e6);

    // cli->Disconnect();
    // while(1){
    //     usleep(1'000'000);
    //     std::cout << "[after disconnect] srv:" << srv->GetServerState()
    //               << " (" << srv->CountClients() << " clis)"
    //               << " cli:" << cli->GetClientState() << std::endl;
    // }


    // ClientConfig cli_conf2{
    //     .server_ip = "127.0.0.1",
    //     .server_port = 5202,
    // };
    IClient* cli2 = factory->createClient(cli_conf);

    cli2->Connect();
    cli->Connect();
    usleep(2*1000);
    std::cout << "[2connect] srv:" << srv->GetServerState()
              << " cli1:" << cli->GetClientState()
              << " cli2:" << cli->GetClientState() << std::endl;

    srv->Stop();

    // cli->SendToSocket(message.data(), message.size());

    usleep(2*1000);
    std::cout << "[srv stop] srv:" << srv->GetServerState()
              << " cli1:" << cli->GetClientState()
              << " cli2:" << cli->GetClientState() << std::endl;
    d("END connection state TEST");
}

// template <typename FactoryMode, int count_ths = 0>
template <typename FactoryMode, int count_ths = 0>
void test2_data_exchange_2var()
{
    // INetworkFactory* factory = new FactoryMode();

    // ServerConfig srv_conf{
    //     .port = 12345,
    //     .max_connections = 10,

    // };
    // ClientConfig cli_conf{
    //     .server_ip = "127.0.0.1",
    //     .server_port = 12345,
    // };
    // IServer* srv = factory->createServer(std::move(srv_conf));
    // IClient* cli = factory->createClient(std::move(cli_conf));

    // srv->start();
    // cli->connect();

    // auto message = generateRandomData(1024);
    // cli->send((char*)message.data(), message.size());

    // srv.on_accept = [](){
    // };
}

template <typename FactoryMode, int count_ths = 0>
void test3_handshake()
{

    //cli send clientid
    d("--START handshake TEST");
    INetworkFactory* factory = new FactoryMode();

    ServerConfig srv_conf{
        .port = 5202,
        .max_connections = 10,
    };
    ClientConfig cli_conf{
        .server_ip = "127.0.0.1",
        .server_port = 5202,
    };
    IServer* srv = factory->createServer(std::move(srv_conf));
    IClient* cli = factory->createClient(std::move(cli_conf));

    srv->Start();
    cli->Connect();

    std::cout << "srv:" << srv->GetServerState() << " cli:" << cli->GetClientState() << std::endl;

    string s("i want check");
    cli->SendToSocket(s.data(), s.size());
}
#include <assert.h>
int main(int argc, char* argv[])
{
    try {
        test1_connection_state<SinglethreadFactory>();
        // test1_connection_state<MultithreadFactory>();
        // test2_data_exchange_2var();
        // test3_handshake();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n\n";
        return 1;
    }

    // while(cli->socket_ > 0){
    //     std::cout << cli->getClientState() << " send:" << cli->stats_.getBitrate() << std::endl;
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // }

    return 0;
}

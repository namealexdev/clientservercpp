#include <netlib.h>

// __FILE__ __FUNCTION__ __PRETTY_FUNCTION__
#define d(x) std::cout << x << " \t(" << __FUNCTION__ << " " << __LINE__ << ")" << std::endl;


template <typename FactoryMode, int count_ths = 0>
void test1_connection_state()
{
    d("START connection state TEST");
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

    std::cout << "srv:" << srv->getServerState() << " cli:" << cli->getClientState() << std::endl;

    bool isstart = srv->start();
    d("server start " << isstart)

    // cli->setAutoSend(1);
    cli->connect();
    usleep(200);

    std::cout << "[after connect] srv:" << srv->getServerState()
              << " (" << srv->countClients() << " clis)"
              << " cli:" << cli->getClientState() << std::endl;

    cli->disconnect();
    usleep(200);
    std::cout << "[after disconnect] srv:" << srv->getServerState()
              << " (" << srv->countClients() << " clis)"
              << " cli:" << cli->getClientState() << std::endl;

    ClientConfig cli_conf2{
        .server_ip = "127.0.0.1",
        .server_port = 5202,
    };
    IClient* cli2 = factory->createClient(std::move(cli_conf2));

    cli2->connect();
    cli->connect();
    usleep(200);
    std::cout << "[2connect] srv:" << srv->getServerState()
              << " cli1:" << cli->getClientState()
              << " cli2:" << cli->getClientState() << std::endl;

    srv->stop();
    usleep(200);
    std::cout << "[srv stop] srv:" << srv->getServerState()
              << " cli1:" << cli->getClientState()
              << " cli2:" << cli->getClientState() << std::endl;
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

void test3_handshake()
{

}

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

#include <netlib.h>

// __FILE__ __FUNCTION__ __PRETTY_FUNCTION__
#define d(x) std::cout << x << " " << __FUNCTION__ << " " << __LINE__ << std::endl;

template <typename FactoryMode, int count_ths = 0>
void test1_connection_state()
{
    d("START connection state TEST");
    INetworkFactory* factory = new FactoryMode();

    ServerConfig srv_conf{
        .port = 12345,
        .max_connections = 10,

    };
    ClientConfig cli_conf{
        .server_ip = "127.0.0.1",
        .server_port = 12345,
    };
    IServer* srv = factory->createServer(srv_conf);
    IClient* cli = factory->createClient(cli_conf);


    std::cout << "srv:" << srv->getServerState() << " cli:" << cli->getClientState() << std::endl;

    srv->start();
    cli->setAutoSend(1);
    cli->connect();
    std::cout << "srv:" << srv->getServerState() << " cli:" << cli->getClientState() << std::endl;

    cli->disconnect();
    std::cout << "srv:" << srv->getServerState() << " cli:" << cli->getClientState() << std::endl;

    IClient* cli2 = factory->createClient(cli_conf);

    cli2->connect();
    cli->connect();
    std::cout << "srv:" << srv->getServerState()
              << " cli1:" << cli->getClientState()
              << " cli2:" << cli->getClientState() << std::endl;

    srv->stop();
    std::cout << "srv:" << srv->getServerState()
              << " cli1:" << cli->getClientState()
              << " cli2:" << cli->getClientState() << std::endl;
    d("END connection state TEST");
}

// template <typename FactoryMode, int count_ths = 0>
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
    // IServer* srv = factory->createServer(srv_conf);
    // IClient* cli = factory->createClient(cli_conf);



    // IClient* cli2 = factory->createClient(cli_conf);

    // cli2->connect();
    // cli->connect();

    // cli2->setAutoSend(0);
    // auto message = generateRandomData(1024);
    // cli2->send_queue(message.data(), message.size());
    // cli2->send(message.data(), message.size());
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

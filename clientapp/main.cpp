// client.cpp

#include <netlib.h>

int main() {

    ClientConfig conf{
        .server_ip = "127.0.0.1",
        .server_port = 12345
    };
    INetworkFactory* factory = new SinglethreadFactory();
    IClient* cli = factory->createClient(conf);
    cli->start();

    return 0;
}

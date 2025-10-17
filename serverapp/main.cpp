// server.cpp
#include <netlib.h>

int main() {

    ServerConfig conf{
        .host = "0.0.0.0",
        .port = 12345
    };
    INetworkFactory* factory = new SinglethreadFactory();
    IServer *srv = factory->createServer(conf);
    srv->start();

    return 0;
}

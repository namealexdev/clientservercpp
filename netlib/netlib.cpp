#include "libinclude/netlib.h"
#include "server.h"
#include "client.h"
#include "client2.h"

std::unique_ptr<IServer> SinglethreadFactory::createServer(ServerConfig conf) {
    return std::make_unique<SimpleServer>(std::move(conf));
}

std::unique_ptr<IClient> SinglethreadFactory::createClient(ClientConfig conf) {
    //SimpleClientEventfd SimpleClient
    return std::make_unique<SimpleClientEventfd>(std::move(conf));
}

std::unique_ptr<IServer> MultithreadFactory::createServer(ServerConfig conf) {
    return std::make_unique<MultithreadServer>(std::move(conf));
}

std::unique_ptr<IClient> MultithreadFactory::createClient(ClientConfig conf) {
    auto client = std::make_unique<SimpleClient>(std::move(conf));
    client->SwitchAsyncQueue(true);
    return client;
}

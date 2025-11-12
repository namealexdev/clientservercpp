#ifndef NETLIB_H
#define NETLIB_H

#include "iserver.h"
#include "iclient.h"

#include <memory>

// factory
class INetworkFactory{
public:
    virtual std::unique_ptr<IServer> createServer(ServerConfig conf) = 0;
    virtual std::unique_ptr<IClient> createClient(ClientConfig conf) = 0;
};

class SinglethreadFactory : public INetworkFactory {
public:
    std::unique_ptr<IServer> createServer(ServerConfig conf) override;
    std::unique_ptr<IClient> createClient(ClientConfig conf) override;
};

class MultithreadFactory : public INetworkFactory {
public:
    std::unique_ptr<IServer> createServer(ServerConfig conf) override;
    std::unique_ptr<IClient> createClient(ClientConfig conf) override;
};


#endif // NETLIB_H

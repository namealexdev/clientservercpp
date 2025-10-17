#ifndef NETLIB_H
#define NETLIB_H

#include "server.h"
#include "client.h"

// factory
class INetworkFactory{
public:
    virtual IServer* createServer(ServerConfig &conf) = 0;
    virtual IClient* createClient(ClientConfig &conf) = 0;
};

class SinglethreadFactory : public INetworkFactory{
public:
    SinglethreadServer* createServer(ServerConfig &conf){
        return new SinglethreadServer(conf);
    }
    SinglethreadClient* createClient(ClientConfig &conf){
        return new SinglethreadClient(conf);
    }
};

class MultithreadFactory : public INetworkFactory{
    MultithreadServer* createServer(ServerConfig &conf){
        return new MultithreadServer(conf);
    }
    MultithreadClient* createClient(ClientConfig &conf){
        return new MultithreadClient(conf);
    }
};


// class Netlib
// {

// public:
//     INetworkFactory *factory_;
// };




#endif // NETLIB_H

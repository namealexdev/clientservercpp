#ifndef NETLIB_H
#define NETLIB_H


#include "server.h"
#include "client.h"


// class IServer;
// class IClient;
// struct ServerConfig;
// struct ClientConfig;
// class SinglethreadServer;
// class SinglethreadClient;

// factory
class INetworkFactory{
public:
    virtual IServer* createServer(ServerConfig&& conf) = 0;
    virtual IClient* createClient(ClientConfig&& conf) = 0;

};

class SinglethreadFactory : public INetworkFactory{
public:
    // SinglethreadServer* createServer(ServerConfig&& conf);
    // SinglethreadClient* createClient(ClientConfig&& conf);

    SinglethreadServer* createServer(ServerConfig&& conf){
        return new SinglethreadServer(std::move(conf));
    }
    SinglethreadClient* createClient(ClientConfig&& conf){
        return new SinglethreadClient(std::move(conf));
    }

};

// class MultithreadFactory : public INetworkFactory{
//     MultithreadServer* createServer(ServerConfig&& conf){
//         return new MultithreadServer(conf);
//     }
//     MultithreadClient* createClient(ClientConfig&& conf){
//         return new MultithreadClient(conf);
//     }
// };



#endif // NETLIB_H

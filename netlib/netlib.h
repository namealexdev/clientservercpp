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

//todo to uniqptr

// factory
class INetworkFactory{
public:
    virtual IServer* createServer(ServerConfig conf) = 0;
    virtual IClient* createClient(ClientConfig conf) = 0;

};

class SinglethreadFactory : public INetworkFactory{
public:
    // SinglethreadServer* createServer(ServerConfig&& conf);
    // SinglethreadClient* createClient(ClientConfig&& conf);

    IServer* createServer(ServerConfig conf){
        return new SimpleServer(std::move(conf));
    }
    IClient* createClient(ClientConfig conf){
        return new SimpleClient(std::move(conf));
    }

};

class MultithreadFactory : public INetworkFactory{
    IServer* createServer(ServerConfig conf){
        return new MultithreadServer(std::move(conf));
    }
    IClient* createClient(ClientConfig conf){
        auto* cli = new SimpleClient(std::move(conf));
        cli->StartAsyncQueue();
        return cli;
    }
};



#endif // NETLIB_H

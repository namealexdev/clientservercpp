#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <libinclude/netlib.h>

#define d(x) std::cout << x << " \t(" << __FUNCTION__ << ' ' << __LINE__ << ")" << std::endl;
#include <signal.h>
#include <atomic>

std::atomic<bool> should_exit{false};

void signal_handler(int signal) {
    should_exit = true;
}
void server_app(){
    signal(SIGINT, signal_handler);

    std::cout << "=== multi SERVER TEST ===" << std::endl;

    //MultithreadFactory SinglethreadFactory
    auto fac = std::make_unique<SinglethreadFactory>();
    ServerConfig srv_conf{
        .port = 12345,
        .max_connections = 10
    };
    std::unique_ptr<IServer> srv = fac->createServer(srv_conf);
    if (!srv){
        std::cerr << "Failed to create server " << std::endl;
        return;
    }
    if (!srv->StartListen(4)) {
        std::cerr << "Failed to start server. err:" << srv->GetLastError() << std::endl;
        return;
    }

    while(!should_exit){
        // static int i = 0;
        // if (i++ >= 5)break;
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // for (auto &w: *srv->GetWorkers()){
        //     d("   " << w->GetStats().getCalcBitrate());
        // }
        srv->GetBitrate();
        // d("srv [" << srv->GetServerState() << "] clis:" << srv->CountClients() << " btr:" << srv->GetBitrate());

    }
    d("end");
}

int main()
{
    server_app();
    return 0;
}

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <libinclude/netlib.h>

#define d(x) std::cout << x << " \t(" << __FUNCTION__ << ' ' << __LINE__ << ")" << std::endl;

void server_app(){
    std::cout << "=== SINGLETHREAD SERVER TEST ===" << std::endl;

    auto fac = std::make_unique<MultithreadFactory>();
    ServerConfig srv_conf{
        .port = 12345,
        .max_connections = 10
    };
    std::unique_ptr<IServer> srv = fac->createServer(srv_conf);
    if (!srv){
        std::cerr << "Failed to create server " << std::endl;
        return;
    }
    if (!srv->StartListen(2)) {
        std::cerr << "Failed to start server. err:" << srv->GetLastError() << std::endl;
        return;
    }

    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // for (auto &w: *srv->GetWorkers()){
        //     d("   " << w->GetStats().getCalcBitrate());
        // }
        srv->GetBitrate();
        // d("srv [" << srv->GetServerState() << "] clis:" << srv->CountClients() << " btr:" << srv->GetBitrate());

    }
}

int main()
{
    server_app();
    return 0;
}

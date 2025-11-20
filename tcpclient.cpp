#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <libinclude/netlib.h>
#define d(x) std::cout << x << " \t(" << __FUNCTION__ << ' ' << __LINE__ << ")" << std::endl;

void client_app(){

    std::cout << "=== SINGLETHREAD CLIENT TEST ===" << std::endl;
    auto fac = std::make_unique<SinglethreadFactory>();
    ClientConfig cli_conf{
        .server_ip = "127.0.0.1",
        .server_port = 12345,
        .auto_send = false,
        .auto_reconnect = true
    };

    std::unique_ptr<IClient> cli = fac->createClient(cli_conf);

    cli->Start();
    d("cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());

    while(cli->GetClientState() != "WAITING"){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        d("0 cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());
    }

    std::vector<char> data1(10'000'000, 'A');// 10MB
    std::vector<char> data2(10'000'000, 'B');// 10MB
    std::vector<char> data3(10'000'000, 'C');// 10MB

    // cli->SwitchAsyncQueue(1);
    // cli->QueueAdd(data1.data(), data1.size());
    // return;

    // сценарии использования. неблокирующего сокета.
    // 1 реконекты. при включении этого в конфиге. и перестать при остановке.
    // 2 разные варианты отправки.
    //      мгновенная(неблок), очередь + отправка, автоотправка при добавлении (в потоке как будет доступно)
    // v1 просто отправка

    // v1
    cli->QueueAdd(data1.data(), data1.size());
    cli->QueueAdd(data2.data(), data2.size());
    cli->QueueAdd(data3.data(), data3.size());
    cli->QueueAdd(data1.data(), data1.size());
    cli->QueueSendAll();// не блокирующий

    // поэтому ждем
    std::atomic_bool stop = false;
    cli->AddHandlerEvent(EventType::WriteReady, [&](void* data){
        d("write ready 1")
            stop = true;
    });
    while(cli->GetClientState() != "WAITING"){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        d("1 cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate() << " " << stop);
    }

    d("cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());

    // v2
    stop = false;
    cli->AddHandlerEvent(EventType::WriteReady, [&](void* data){
        d("write ready 2")
            stop = true;
    });
    cli->SwitchAsyncQueue(1);// надо ли динамически возможность включать?

    cli->QueueAdd(data1.data(), data1.size());
    cli->QueueAdd(data2.data(), data2.size());
    cli->QueueAdd(data3.data(), data3.size());
    cli->QueueAdd(data1.data(), data1.size());

    while(cli->GetClientState() != "WAITING"){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        d("2 cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());
    }
    d("cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());
    d("after send");
}

void client_stress(){

    auto fac = std::make_unique<SinglethreadFactory>();
    ClientConfig cli_conf{
        .server_ip = "127.0.0.1",
        .server_port = 12345,
        .auto_send = true,
        .auto_reconnect = true
    };

    std::unique_ptr<IClient> cli = fac->createClient(cli_conf);

    cli->Start();
    d("cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getBitrate());

    std::vector<char> data100(100'000'000, 'A');// 1Gb

    // while(1){
    //     std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // };
    new std::thread([&](){
        while (1){
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            d("[stress] cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getCalcBitrate());
        }
    });

    //async queue
    while (1){
        for (int i = 0; i < 100; i++){
            cli->QueueAdd(data100.data(), data100.size());
        }
        // d("[stress async] cli [" << cli->GetClientState() << "] btr:" << cli->GetStats().getCalcBitrate());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    //sync
    // cli->SwitchAsyncQueue(false);
    // while (1){
    //     if (!cli->IsConnected()){
    //         std::this_thread::yield();
    //         continue;
    //     }
    //     for (int i = 0; i < 10; i++){
    //         cli->QueueAdd(data100.data(), data100.size());
    //     }
    //     cli->QueueSendAll();
    // }

}

int main()
{
    client_stress();
    // client_app();
    return 0;
}

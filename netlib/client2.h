// simple_client_eventfd.cpp
#include "client.h"          // IClient, ClientConfig, logger d()
#include <boost/lockfree/spsc_queue.hpp>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <memory>
#include <iostream>


class SimpleClientEventfd : public IClient {
public:
    struct QueueItem {
        char*  data;
        size_t size = 0;
        size_t sent_bytes = 0;  // сколько байт уже отправлено
    };

    explicit SimpleClientEventfd(ClientConfig cfg);
    ~SimpleClientEventfd() override;

    void Start() override;
    void Stop() override;

    bool QueueAdd(char* data, int size) override;
    bool QueueSendAll() override;

    int SendToSocket(char* d, uint32_t sz) override;
    void SwitchAsyncQueue(bool enable) override;

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler) override;

private:
    int epfd_      = -1;
    int event_fd_  = -1;
    int socket_ = -1;

    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> epoll_thread_;
    EventDispatcher* dispatcher_ = 0;

    boost::lockfree::spsc_queue<QueueItem> queue_{QUEUE_CAP};

    void epollLoop();

    bool reconnect();

    void onSocketClosed();
    void closeSocket();

    bool enableEpollOut();
    bool disableEpollOut();

    // handleData перенесена сюда
    void handleData();

    char buffer_[BUF_READ_SIZE];
};

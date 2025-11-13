#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "epoll.h"
#include "libinclude/iclient.h"

#include <condition_variable>
#include <mutex>

class SimpleClient : public IClient{
public:
    SimpleClient(ClientConfig config);
    ~SimpleClient();

    void Connect();
    void Disconnect();

    int SendToSocket(char* data, uint32_t size);
    void QueueAdd(char* data, int size);
    void QueueSend();
    void StartAsyncQueue();

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleData();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    char buffer_[BUF_READ_SIZE];// for recv data from server

    // WARN: поток и мутекс нужны только для мультипотока
    // TODO: change to lockfree (чтобы убрать mutex)
    std::thread* queue_th_;
    std::mutex queue_mtx_;
    std::queue<std::pair<char*,int>> queue_;
};


#endif // CLIENT_H

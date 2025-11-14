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

    // чтобы сразу начинать и автоматом делать реконект (асинхронный)
    void Start();
    // sync connect и start
    void StartWaitConnect();
    void Stop();

    int SendToSocket(char* data, uint32_t size);
    void QueueAdd(char* data, int size);
    // sync send
    bool QueueSendAll();
    void SwitchAsyncQueue(bool enable);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleData();
    void reconnect();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    char buffer_[BUF_READ_SIZE];// for recv data from server

    // WARN: поток и мутекс нужны только для мультипотока
    // sync push pop для потока
    // TODO: change to lockfree (чтобы убрать mutex)
    std::thread* queue_th_;
    std::mutex queue_mtx_;
    struct QueueItem {
        char* data;
        int size = 0;
        int sent_bytes = 0;  // сколько байт уже отправлено
    };
    std::queue<QueueItem> queue_;
    std::condition_variable send_queue_cv_;
    bool async_queue_send_ = false;

    bool queueSendAllUnsafe();
};


#endif // CLIENT_H

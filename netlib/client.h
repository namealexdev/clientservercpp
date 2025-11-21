#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "epoll.h"
#include "libinclude/iclient.h"

#include <condition_variable>
#include <mutex>

// #include "facade_interface_queue.h"

#pragma push_macro("d")
#undef d
#include <boost/lockfree/spsc_queue.hpp>
#pragma pop_macro("d")

#include <linux/futex.h>
#include <sys/syscall.h>

struct QueueItem {
    char* data;
    int size = 0;
    int sent_bytes = 0;  // сколько байт уже отправлено
};

class SimpleClient : public IClient{
public:
    explicit SimpleClient(ClientConfig config);
    ~SimpleClient();

    void Start() override;
    void Stop() override;

    int SendToSocket(char* data, uint32_t size) override;
    bool QueueAdd(char* data, int size) override;
    bool QueueSendAll() override;
    void SwitchAsyncQueue(bool enable) override;

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler) override;

private:

    void handleData();
    void reconnect();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    // TODO(пока не используется): for recv data from server (что приходит?)
    char buffer_[BUF_READ_SIZE];

    std::thread* queue_th_ = 0;
    bool async_queue_send_ = false;

    // std::mutex queue_mtx_;
    // std::queue<QueueItem> queue_;
    boost::lockfree::spsc_queue<QueueItem> queue_{QUEUE_CAP};

    // обертки для очереди (надо?)
    bool push_item(const QueueItem&& item);
    QueueItem& front_item();
    bool pop_item(QueueItem& item);
    bool is_queue_empty();

    // для избавления от busy loop при использовании асинхронной очереди (когда нету пакетов)
    std::atomic<int> futex_flag_{0};// 0 = sleep, 1 = wakeup
    void futex_wake_queue() noexcept;
    void futex_wait_queue() noexcept;
};


#endif // CLIENT_H

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


class SimpleClient : public IClient{
public:
    explicit SimpleClient(ClientConfig config);
    ~SimpleClient();

    void Start();
    void Stop();

    int SendToSocket(char* data, uint32_t size);
    bool QueueAdd(char* data, int size);
    bool QueueSendAll();
    void SwitchAsyncQueue(bool enable);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleData();
    void reconnect();

    int socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;

    // TODO(пока без реализации): for recv data from server
    char buffer_[BUF_READ_SIZE];

    std::thread* queue_th_ = 0;
    // std::condition_variable send_queue_cv_;
    bool async_queue_send_ = false;

    struct QueueItem {
        char* data;
        int size = 0;
        int sent_bytes = 0;  // сколько байт уже отправлено
    };
    // std::mutex queue_mtx_;
    // std::queue<QueueItem> queue_;
    boost::lockfree::spsc_queue<QueueItem> queue_{100};

    // обертки для очереди
    bool push_item(const QueueItem&& item);
    QueueItem& front_item();
    bool pop_item(QueueItem& item);
    bool is_queue_empty();

    // для избавления от busy loop при использовании асинхронной очереди
    std::atomic<int> futex_flag_{0};// 0 = sleep, 1 = wakeup
    void futex_wake_queue() noexcept;
    void futex_wait_queue() noexcept;
};


#endif // CLIENT_H

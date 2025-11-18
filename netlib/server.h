#ifndef SERVER_H
#define SERVER_H

#include "const.h"
#include "epoll.h"

#include <cassert>
#include <condition_variable>
#include <mutex>

#include "libinclude/iserver.h"

struct PacketParser {
    std::vector<char> data;           // Буфер для данных (заголовок + полезная нагрузка)
    uint32_t payload_size = 0;        // Размер полезной нагрузки в хостовом порядке
    uint32_t bytes_received = 0;      // Общее количество полученных байт
    bool header_parsed = false;       // Флаг, что заголовок распарсен

    PacketParser() {
        data.resize(PACKET_HEADER_SIZE);
    }

    inline void Reset() {
        data.clear();
        payload_size = 0;
        bytes_received = 0;
        header_parsed = false;
    }

    // Сохраняет данные пакета если они есть
    // Возвращает количество прочитанных байт
    int ParseDataPacket(const char* buf, int sz) {
        int total_read = 0;

        while (total_read < sz) {
            if (!header_parsed) {
                int header_remaining = PACKET_HEADER_SIZE - bytes_received;
                int to_copy = std::min(sz - total_read, header_remaining);

                // копируем прямо в data
                std::memcpy(data.data() + bytes_received, buf + total_read, to_copy);
                bytes_received += to_copy;
                total_read += to_copy;

                if (bytes_received == PACKET_HEADER_SIZE) {
                    uint32_t net_size;
                    std::memcpy(&net_size, data.data(), sizeof(net_size));
                    payload_size = ntohl(net_size);

                    // резервируем сразу память под полный пакет
                    data.resize(PACKET_HEADER_SIZE + payload_size);
                    header_parsed = true;
                }
            }

            if (header_parsed) {
                int payload_received = bytes_received - PACKET_HEADER_SIZE;
                int payload_remaining = payload_size - payload_received;
                if (payload_remaining > 0) {
                    int to_copy = std::min(sz - total_read, payload_remaining);
                    std::memcpy(data.data() + bytes_received, buf + total_read, to_copy);
                    bytes_received += to_copy;
                    total_read += to_copy;
                }

                if (IsPacketReady()) {
                    // пакет готов
                    break; // можно обработать пакет снаружи
                }
            }
        }

        return total_read;
    }


    inline bool IsPacketReady() const {
        return header_parsed &&
               (bytes_received >= PACKET_HEADER_SIZE + payload_size);
    }

    // Вспомогательные методы для доступа к данным
    inline const char* GetPayloadData() const {
        return data.data() + PACKET_HEADER_SIZE;
    }

    inline uint32_t GetPayloadSize() const {
        return payload_size;
    }

    inline const std::vector<char>& GetFullPacket() const {
        return data;
    }
};

struct ClientData{
    // только для handshake?
    // enum ClientState {
    //     HANDSHAKE,
    //     DATA
    // } state;
    // std::array<uint8_t, 16> client_uuid;

    Stats stats;
    PacketParser pktReader_;
    // очередь пакетов для клиента?
};


class SimpleServer : public IServer{
public:
    SimpleServer(ServerConfig config, EventDispatcher* e = nullptr);
    bool StartListen(int num_workers = 0);
    void StartWait();
    void Stop();
    int CountClients();

    // нужен для передачи сокетов в многопоточном сервере
    void AddClientFd(int fd, const Stats& st);

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

    double GetBitrate(){
        // поидее только один
        std::vector<Stats*> stats = GetClientsStats();
        double btr = 0;
        int i = 1;
        for(auto& c: stats){
            // для accept сокета, тоже добавляем в клиенты чтобы статистику
            // if (c->ip.empty())continue;
            d(i++ << " " << c->ip << " " << c->getCalcBitrate() << " " << c->total_bytes);
            btr += c->getBitrate();
        }
        return btr;
        // Stats& stats = GetStats();
        // stats.calcBitrate();
        // d("sim get btr " << stats.getBitrate() << " " << stats.total_bytes << "(" << stats.ip << ")")
        // return stats.getBitrate();
    };
    std::vector<std::unique_ptr<IServer>>* GetWorkers(){
        return nullptr;
    };
    std::vector<Stats*> GetClientsStats(){
        std::vector<Stats*> vec;
        for (auto& c: clients_){
            vec.emplace_back(&c.second->stats);
        }
        return vec;
    }

private:
    void handleAccept(); // принимает клиентов
    void handleClientData(int fd); // обрабатывает данные от клиента
    void removeClient(int fd); // удаляет клиента из epoll и списка

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll epoll_;
    char buffer_[BUF_READ_SIZE];

    std::unordered_map<int, std::shared_ptr<ClientData>> clients_;
    //for add client (from other thread when MultithreadServer) and erace
    std::mutex clients_mtx_;


};

class MultithreadServer : public IServer{
public:
    explicit MultithreadServer(ServerConfig config);

    bool StartListen(int num_workers = 1);
    void Stop();
    int CountClients();

    void AddClientFd(int fd, const Stats &st){
        // сюда не должно быть таких конектов
        assert(false);
        // accept_epoll_.AddFd(fd);
        // worker_client_counts_[0] ++;
        // preClient_socks_.push(std::make_pair(fd, std::move(st)));
    }

    double GetBitrate(){
        // d("multi get btr:")
        double bps = 0;
        int num_worker = 1;
        for (auto &w: workers_){
            std::vector<Stats*> stats = w->GetClientsStats();
            for(auto& c: stats){
                d(num_worker << "-  " << c->ip << " " << c->getCalcBitrate());
                bps += c->getBitrate();
            }
            num_worker++;
        }
        d("full " << Stats::formatBitrate(bps))
        return bps;
    };
    std::vector<std::unique_ptr<IServer>>* GetWorkers(){
        return &workers_;
    };
    std::vector<Stats*> GetClientsStats(){
        return {};
    }

    void AddHandlerEvent(EventType type, std::function<void(void*)> handler);

private:
    void handleAccept(int fd); // распределяет сокеты по воркерам

    int listen_socket_ = -1;
    EventDispatcher* dispatcher_ = 0;
    BaseEpoll accept_epoll_; // только для accept

    std::vector<std::unique_ptr<IServer>> workers_;
    std::vector<std::atomic_int> worker_client_counts_; // для балансировки(чтобы без atomic)
};

#endif // SERVER_H

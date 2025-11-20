#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "const.h"
#include <array>

#pragma pack(push,1)
struct ClientHiMsg {
    std::array<uint8_t, 16> uuid;
};

struct ServerAnsHiMsg {
    enum ClientMode : uint8_t {
        ERRUUID = 0,
        SEND    = 1
    };
    std::array<uint8_t, 16> client_uuid;
    ClientMode client_mode;
};
#pragma pack(pop)

class SyncHandshake {
public:
    explicit SyncHandshake(int fd)
        : fd_(fd)
    {}

    // --- отправка и получение строго size+payload блокирующие ---
    bool sendBlock(const void* data, size_t size) {
        uint32_t net_size = htonl((uint32_t)size);

        // отправляем size + payload синхронно
        if (!sendAll(&net_size, sizeof(net_size))) return false;
        if (!sendAll(data, size)) return false;
        return true;
    }

    bool recvBlock(std::vector<uint8_t>& payload) {
        uint32_t net_size = 0;
        if (!recvAll(&net_size, sizeof(net_size))) return false;
        uint32_t size = ntohl(net_size);

        payload.resize(size);
        if (!recvAll(payload.data(), size)) return false;
        return true;
    }

    // --- high-level handshake ---
    bool clientHandshake(const ClientHiMsg& hi,
                         ServerAnsHiMsg& ans,
                         int timeout_ms)
    {
        if (!setTimeout(timeout_ms)) return false;

        if (!sendBlock(&hi, sizeof(hi))) return restoreTimeout(), false;

        std::vector<uint8_t> buf;
        if (!recvBlock(buf)) return restoreTimeout(), false;

        if (buf.size() != sizeof(ServerAnsHiMsg))
            return restoreTimeout(), false;

        memcpy(&ans, buf.data(), sizeof(ans));

        return restoreTimeout(), true;
    }

    bool serverHandshake(ClientHiMsg& client_hi,
                         ServerAnsHiMsg& answer,
                         int timeout_ms)
    {
        if (!setTimeout(timeout_ms)) return false;

        // ждём блокирующе клиентский hello
        std::vector<uint8_t> buf;
        if (!recvBlock(buf)) return restoreTimeout(), false;

        if (buf.size() != sizeof(ClientHiMsg))
            return restoreTimeout(), false;

        memcpy(&client_hi, buf.data(), sizeof(client_hi));

        // далее сервер формирует answer сам
        if (!sendBlock(&answer, sizeof(answer)))
            return restoreTimeout(), false;

        return restoreTimeout(), true;
    }

private:
    int fd_;
    timeval old_send_{}, old_recv_{};
    bool has_old_ = false;

    // установка таймаута на время handshake
    bool setTimeout(int ms) {
        if (!has_old_) {
            socklen_t sl = sizeof(old_send_);
            getsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &old_send_, &sl);
            getsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &old_recv_, &sl);
            has_old_ = true;
        }

        timeval tv{};
        tv.tv_sec  = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;

        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        return true;
    }

    bool restoreTimeout() {
        if (!has_old_) return true;
        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &old_send_, sizeof(old_send_));
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &old_recv_, sizeof(old_recv_));
        has_old_ = false;
        return true;
    }

    // блокирующая гарантированная отправка
    bool sendAll(const void* data, size_t size) {
        const uint8_t* p = (const uint8_t*)data;
        size_t left = size;

        while (left > 0) {
            ssize_t s = send(fd_, p, left, 0);
            if (s <= 0) return false;
            left -= s;
            p    += s;
        }
        return true;
    }

    // блокирующее гарантированное получение
    bool recvAll(void* data, size_t size) {
        uint8_t* p = (uint8_t*)data;
        size_t left = size;

        while (left > 0) {
            ssize_t r = recv(fd_, p, left, 0);
            if (r <= 0) return false;
            left -= r;
            p    += r;
        }
        return true;
    }
};


#endif // HANDSHAKE_H

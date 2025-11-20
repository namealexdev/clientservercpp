#ifndef PACKETPARSER_H
#define PACKETPARSER_H

#include "const.h"

struct PacketParser {
    std::vector<char> data;           // Буфер для данных (заголовок + payload)
    uint32_t payload_size = 0;
    uint32_t bytes_received = 0;
    bool header_parsed = false;

    PacketParser() {
        data.reserve(PACKET_HEADER_SIZE + 1024);
        data.resize(PACKET_HEADER_SIZE);
    }

    inline void Reset() {
        payload_size = 0;
        bytes_received = 0;
        header_parsed = false;
        data.resize(PACKET_HEADER_SIZE);
    }

    inline void EnsureCapacity(size_t needed) {
        if (data.capacity() < needed) {
            data.reserve(needed);
        }
        if (data.size() < needed) {
            data.resize(needed); // resize logical size when we know exact needed
        }
    }

    // Сохраняет данные пакета если они есть
    // Возвращает сколько байт прочитано из буфера
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
                    std::memcpy(&net_size, data.data(), sizeof(net_size));// ровно 4 байта

                    net_size = (uint32_t)*data.data();

                    payload_size = ntohl(net_size);

                    // TODO(): проверка на максимальый или очень большой размер

                    // резервируем сразу память под полный пакет
                    // data.resize(PACKET_HEADER_SIZE + payload_size);
                    EnsureCapacity(PACKET_HEADER_SIZE + payload_size);
                    header_parsed = true;
                }
            }

            if (header_parsed) {
                int payload_received = bytes_received - PACKET_HEADER_SIZE;
                int payload_remaining = payload_size - payload_received;
                if (payload_remaining > 0) {
                    int to_copy = std::min(sz - total_read, payload_remaining);
                    // std::memcpy(data.data() + bytes_received, buf + total_read, to_copy);
                    bytes_received += to_copy;
                    total_read += to_copy;
                }

                if (IsPacketReady()) {
                    // пакет готов, можно обработать пакет снаружи
                    break;
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

#endif // PACKETPARSER_H

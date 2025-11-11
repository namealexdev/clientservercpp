#include "utils.h"
#include "const.h"
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <filesystem>
#include <array>

std::string uuid_to_string(const std::array<uint8_t, 16>& uuid) {
    char buf[37]; // 36 символов + '\0'
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  uuid[0], uuid[1], uuid[2], uuid[3],
                  uuid[4], uuid[5],
                  uuid[6], uuid[7],
                  uuid[8], uuid[9],
                  uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return std::string(buf);
}

int getRandomNumber(int from, int to) {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<int> dis(from, to);
    return dis(gen);
}

std::vector<uint8_t> generateRandomData(size_t size)
{
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    for (size_t i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }
    return data;
}


void write2file(string& sfilename, const char* data, ssize_t size) {
    static const char* filename = sfilename.c_str();
    static const off_t MAX_SIZE = 1ULL * 1024 * 1024 * 1024; // 1 GiB
    static int fd = -1;

    if (fd == -1) {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND , 0644);//O_SYNC
        if (fd == -1) return;
    }

    // check size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd); fd = -1;
        return;
    }
    if (st.st_size >= MAX_SIZE) {
        ftruncate(fd, 0); // reset
    }

    //full write
    int write_size = 0;
    while(write_size != size){
        int wr = write(fd, data+write_size, size-write_size);
        if (wr <= 0) {
            std::cerr << "write to file failed" << std::endl;
            break;
        }
        write_size += wr;
    }

    fsync(fd);//flush
}



std::array<uint8_t, 16> generateUuid() {
    static std::random_device rd;
    static std::mt19937_64 gen;
    static std::uniform_int_distribution<uint64_t> dis;
    std::array<uint8_t, 16> uuid;

    // Генерируем 128 бит случайных данных (16 байт)
    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    std::memcpy(uuid.data(), &part1, 8);
    std::memcpy(uuid.data() + 8, &part2, 8);

    // Устанавливаем версию UUID (версия 4 - случайный UUID)
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // version 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // variant

    // toString XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    // std::stringstream ss;
    // ss << std::hex << std::setfill('0');
    // for (size_t i = 0; i < 16; ++i) {
    //     ss << std::setw(2) << static_cast<unsigned>(uuid[i]);
    //     if (i == 3 || i == 5 || i == 7 || i == 9) {
    //         ss << "-";
    //     }
    // }
    // return ss.str();
    return uuid;
}

bool write_session_uuid(const std::array<uint8_t, 16>& client_session_uuid,
                        const std::string& filename) {
    std::ofstream file(filename, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(client_session_uuid.data()), client_session_uuid.size());
    return file.good();
}

bool read_session_uuid(const std::string& filename, std::array<uint8_t, 16>& result) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    // read 16 bytes
    file.read(reinterpret_cast<char*>(result.data()), result.size());
    // validate
    bool valid_size = file.gcount() == result.size();
    if (valid_size){
        bool version_ok = (result[6] & 0xF0) == 0x40; // version 4
        bool variant_ok = (result[8] & 0xC0) == 0x80; // variant 1
        return version_ok && variant_ok;
    }
    return false;

    // std::streamsize size = file.tellg();
    // file.seekg(0, std::ios::beg);
    // return !!file.read(result.data(), size);
}

// #include <sys/socket.h>
// bool isSocketAlive(int sockfd) {
//     if (sockfd == -1) return false;

//     // Проверка с помощью poll с нулевым таймаутом
//     struct pollfd pfd = {0};
//     pfd.fd = sockfd;
//     pfd.events = POLLERR | POLLHUP;

//     int result = poll(&pfd, 1, 0);
//     if (result < 0) return false;
//     if (result > 0 && (pfd.revents & (POLLERR | POLLHUP))) return false;

//     return true;
// }

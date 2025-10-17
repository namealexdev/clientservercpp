#include "utils.h"
#include "const.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int getRandomNumber(int from, int to) {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<int> dis(from, to);
    return dis(gen);
}

void write2file(const char* data, ssize_t size) {
    static const char* filename = "output.txt";
    static const off_t MAX_SIZE = 1ULL * 1024 * 1024 * 1024; // 1 GiB
    static int fd = -1;

    if (fd == -1) {
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0644);
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

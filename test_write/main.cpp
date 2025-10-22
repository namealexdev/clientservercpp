#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>

int write_to_stdout(const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t written = 0;
    ssize_t n;
    int count = 1;
    while (written < len) {
        n = write(STDOUT_FILENO, p + written, len - written);
        std::cout << count++ << " write " << n << std::endl;
        if (n == -1) {
            if (errno == EINTR) {
                continue; // прерван сигналом — повторить
            }
            return -1;
        }
        written += n;
    }
    return 0;
}

int main() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс: читаем из pipe
        close(pipefd[1]); // закрываем запись
        char buffer[2];
        while (read(pipefd[0], buffer, sizeof(buffer)) > 0) {
            // читаем по 2 байта, чтобы буфер освобождался медленно
        }
        close(pipefd[0]);
        return 0;
    } else if (pid > 0) {
        // Родитель: перенаправляем stdout в pipe
        close(pipefd[0]); // закрываем чтение
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        const char* msg = "Это длинное сообщение, которое будет записываться частями.\n";
        write_to_stdout(msg, strlen(msg));

        close(STDOUT_FILENO); // закрываем, чтобы дочерний процесс завершил чтение
        wait(NULL);           // ждём завершения дочернего процесса
    } else {
        perror("fork");
        return 1;
    }

    return 0;
}

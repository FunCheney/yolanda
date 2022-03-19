#include "lib/common.h"

#define MAX_LINE 4096

char
rot13_char(char c) {
    if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M'))
        return c + 13;
    else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z'))
        return c - 13;
    else
        return c;
}

void child_run(int fd) {
    char outbuf[MAX_LINE + 1];
    size_t outbuf_used = 0;
    ssize_t result;

    while (1) {
        char ch;
        result = recv(fd, &ch, 1, 0);
        if (result == 0) {
            break;
        } else if (result == -1) {
            perror("read");
            break;
        }

        /* We do this test to keep the user from overflowing the buffer. */
        if (outbuf_used < sizeof(outbuf)) {
            outbuf[outbuf_used++] = rot13_char(ch);
        }

        if (ch == '\n') {
            send(fd, outbuf, outbuf_used, 0);
            outbuf_used = 0;
            continue;
        }
    }
}


void sigchld_handler(int sig) {
    // 在一个循环体内调用了 waitpid 函数，以便回收所有已终止的子进程
    // 项 WNOHANG 用来告诉内核，即使还有未终止的子进程也不要阻塞在 waitpid 上。
    // 注意这里不可以使用 wait，因为 wait 函数在有未终止子进程的情况下，没有办法不阻塞。
    while (waitpid(-1, 0, WNOHANG) > 0);
    return;
}

int main(int c, char **v) {
    int listener_fd = tcp_server_listen(SERV_PORT);
    // 注册一个信号处理函数，用来回收子进程资源；
    signal(SIGCHLD, sigchld_handler);
    while (1) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int fd = accept(listener_fd, (struct sockaddr *) &ss, &slen);
        if (fd < 0) {
            error(1, errno, "accept failed");
            exit(1);
        }

        if (fork() == 0) {
            // fork 的返回值为 0，进入子进程处理逻辑
            // 子进程不需要关心监听套接字，故而在这里关闭掉监听套接字 listen_fd
            close(listener_fd);
            // 之后调用 child_run 函数使用已连接套接字 fd 来进行数据读写
            child_run(fd);
            exit(0);
        } else {
            // 进入的是父进程处理逻辑，父进程不需要关心连接套接字，所以在这里关闭连接套接字。
            /**
             * 从父进程派生出的子进程，同时也会复制一份描述字，也就是说，连接套接字和监听套接字的引用计数都会被加 1，
             * 而调用 close 函数则会对引用计数进行减 1 操作，这样在套接字引用计数到 0 时，才可以将套接字资源回收。
             * 所以，这里的 close 函数非常重要，缺少了它们，就会引起服务器端资源的泄露。
             */
            close(fd);
        }
    }

    return 0;
}
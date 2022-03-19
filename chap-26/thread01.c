#include "lib/common.h"

extern void loop_echo(int);

void thread_run(void *arg) {
    // 子线程转变为分离的，也就意味着子线程独自负责线程资源回收
    pthread_detach(pthread_self());
    // 强制将指针转变为描述符数据
    int fd = (int) arg;
    loop_echo(fd);
}

int main(int c, char **v) {
    int listener_fd = tcp_server_listen(SERV_PORT);
    pthread_t tid;

    while (1) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        // 阻塞调用在 accept 上，一旦有新连接建立，阻塞调用返回，调用 pthread_create 创建一个子线程来处理这个连接
        int fd = accept(listener_fd, (struct sockaddr *) &ss, &slen);
        if (fd < 0) {
            error(1, errno, "accept failed");
        } else {
            // 这里通过强制把描述字转换为 void * 指针的方式，完成了传值
            pthread_create(&tid, NULL, &thread_run, (void *) fd);
        }
    }

    return 0;
}
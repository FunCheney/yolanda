#include "lib/common.h"

#define INIT_SIZE 128

int main(int argc, char **argv) {
    int listen_fd, connected_fd;
    int ready_number;
    ssize_t n;
    char buf[MAXLINE];
    struct sockaddr_in client_addr;
    // 创建一个监听套接字，并绑定在本地的 地址 合端口上
    listen_fd = tcp_server_listen(SERV_PORT);

    //初始化 pollfd 数组，这个数组的第一个元素是 listen_fd，其余的用来记录将要连接的 connect_fd
    struct pollfd event_set[INIT_SIZE];
    event_set[0].fd = listen_fd;
    // 将监听套接字 listen_fd 合对应的 POLLRDNORM 事件加入到 event_set 里面 表示我们期望系统内核检测监听套接字上的连接建立完成事件
    event_set[0].events = POLLRDNORM;

    // 用-1表示这个数组位置还没有被占用
    int i;
    for (i = 1; i < INIT_SIZE; i++) {
        // poll 函数中对文件描述为负数的 描述符忽略，从 1 开始，因为 0 给了 listen_fd
        event_set[i].fd = -1;
    }

    for (;;) {
        // 调用 poll 函数检测事件
        // poll 函数传入的参数为 event_set 数组，数组大小 INIT_SIZE 和 -1
        // 这里之所以传入 INIT_SIZE，是因为 poll 函数已经能保证可以自动忽略 fd 为 -1 的 pollfd，
        // 否则我们每次都需要计算一下 event_size 里真正需要被检测的元素大小；
        // timeout 设置为 -1，表示在 I/O 事件发生之前 poll 调用一直阻塞。
        if ((ready_number = poll(event_set, INIT_SIZE, -1)) < 0) {
            error(1, errno, "poll failed ");
        }

        // 使用了如 event_set[0].revent 来和对应的事件类型进行位与操作
        // 这是因为 event 都是通过二进制位来进行记录的，位与操作是和对应的二进制位进行操作，一个文件描述字是可以对应到多个事件类型的。
        if (event_set[0].revents & POLLRDNORM) {
            socklen_t client_len = sizeof(client_addr);
            // 调用 accept 函数获取了连接描述字
            connected_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);

            //找到一个可以记录该连接套接字的位置
            for (i = 1; i < INIT_SIZE; i++) {
                // 就是把连接描述字 connect_fd 也加入到 event_set 里，
                // 而且说明了我们感兴趣的事件类型为 POLLRDNORM，也就是套接字上有数据可以读
                // 们从数组里查找一个没有没占用的位置，也就是 fd 为 -1 的位置，然后把 fd 设置为新的连接套接字 connect_fd。
                if (event_set[i].fd < 0) {
                    event_set[i].fd = connected_fd;
                    event_set[i].events = POLLRDNORM;
                    break;
                }
            }

            // 说明我们的 event_set 已经被很多连接充满了，没有办法接收更多的连接了
            if (i == INIT_SIZE) {
                error(1, errno, "can not hold so many clients");
            }

            // 因为 poll 返回的一个整数，说明了这次 I/O 事件描述符的个数，
            // 如果处理完监听套接字之后，就已经完成了这次 I/O 复用所要处理的事情，
            // 那么我们就可以跳过后面的处理，再次进入 poll 调用。
            if (--ready_number <= 0)
                continue;
        }

        for (i = 1; i < INIT_SIZE; i++) {
            int socket_fd;
            // 如果数组里的 pollfd 的 fd 为 -1，说明这个 pollfd 没有递交有效的检测，直接跳过
            if ((socket_fd = event_set[i].fd) < 0)
                continue;
            if (event_set[i].revents & (POLLRDNORM | POLLERR)) {
            // 通过检测 revents 的事件类型是 POLLRDNORM 或者 POLLERR，我们可以进行读操作
                if ((n = read(socket_fd, buf, MAXLINE)) > 0) {
                    // 读取数据正常之后，再通过 write 操作回显给客户端
                    if (write(socket_fd, buf, n) < 0) {
                        error(1, errno, "write error");
                    }
                } else if (n == 0 || errno == ECONNRESET) {
                    // 如果读到 EOF 或者是连接重置，则关闭这个连接，并且把 event_set 对应的 pollfd 重置
                    close(socket_fd);
                    event_set[i].fd = -1;
                } else {
                    // 取数据失败
                    error(1, errno, "read error");
                }
                // 是判断如果事件已经被完全处理完之后，直接跳过对 event_set 的循环处理，再次来到 poll 调用
                if (--ready_number <= 0)
                    break;
            }
        }
    }
}

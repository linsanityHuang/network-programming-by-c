#include "lib/common.h"

#define INIT_SIZE 128

int main(int argc, char **argv)
{
    int listen_fd, connected_fd;
    int ready_number;
    ssize_t n;
    char buf[MAXLINE];
    struct sockaddr_in client_addr;

    // 创建一个监听套接字，并绑定在本地的地址和端口上
    listen_fd = tcp_server_listen(SERV_PORT);

    // struct pollfd
    // {
    //     int fd;          /* file descriptor */
    //     short events;    /* events to look for */
    //     short revents;   /* events returned */
    // };

    // 可读事件
    // #define POLLIN 0x0001     /* any readable data available */
    // #define POLLPRI 0x0002    /* OOB/Urgent readable data */
    // #define POLLRDNORM 0x0040 /* non-OOB/URG data available */
    // #define POLLRDBAND 0x0080 /* OOB/Urgent readable data */

    // 可写事件
    // #define POLLOUT 0x0004     /* file descriptor is writeable */
    // #define POLLWRNORM POLLOUT /* no write type differentiation */
    // #define POLLWRBAND 0x0100  /* OOB/Urgent data can be written */

    // 错误事件
    // #define POLLERR 0x0008  /* 一些错误发送 */
    // #define POLLHUP 0x0010  /* 描述符挂起*/
    // #define POLLNVAL 0x0020 /* 请求的事件无效*/

    // 初始化pollfd数组，这个数组的第一个元素是listen_fd，其余的用来记录将要连接的connect_fd
    // 这里数组的大小固定为 INIT_SIZE，这在实际的生产环境肯定是需要改进的。
    struct pollfd event_set[INIT_SIZE];

    // 将监听套接字 listen_fd 和对应的 POLLRDNORM 事件加入到 event_set 里，
    // 表示我们期望系统内核检测监听套接字上的连接建立完成事件
    event_set[0].fd = listen_fd;
    event_set[0].events = POLLRDNORM;

    // 用-1表示这个数组位置还没有被占用
    // 将 event_set 数组里其他没有用到的 fd 统统设置为 -1。
    // 这里 -1 也表示了当前 pollfd 没有被使用的意思
    // poll 函数将会忽略这个 pollfd
    int i;
    for (i = 1; i < INIT_SIZE; i++)
    {
        event_set[i].fd = -1;
    }

    for (;;)
    {
        // int poll(struct pollfd *fds, unsigned long nfds, int timeout);
        // 返回值：若有就绪描述符则为其数目，若超时则为0，若出错则为-1
        if ((ready_number = poll(event_set, INIT_SIZE, -1)) < 0)
        {
            error(1, errno, "poll failed ");
        }

        // 系统内核检测到监听套接字上的连接建立事件
        if (event_set[0].revents & POLLRDNORM)
        {
            socklen_t client_len = sizeof(client_addr);

            // 调用 accept 函数获取连接描述字
            connected_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

            //找到一个可以记录该连接套接字的位置
            for (i = 1; i < INIT_SIZE; i++)
            {
                if (event_set[i].fd < 0)
                {
                    // 把连接描述字 connect_fd 也加入到 event_set 里
                    // 而且说明了我们感兴趣的事件类型为 POLLRDNORM
                    // 也就是套接字上有数据可以读
                    event_set[i].fd = connected_fd;
                    event_set[i].events = POLLRDNORM;
                    break;
                }
            }

            if (i == INIT_SIZE)
            {
                error(1, errno, "can not hold so many clients");
            }

            // 一个加速优化能力
            // 因为 poll 返回的一个整数, 说明了这次 I/O 事件描述符的个数,
            // 如果处理完监听套接字之后，就已经完成了这次 I/O 复用所要处理的事情,
            // 即ready_number = 1
            // 那么我们就可以跳过后面的处理(连接套接字的读写事件)，再次进入 poll 调用
            // 因为这个分支是监听套接字的可读事件处理逻辑且只有一个事件, 那说明只有监听套接字一个事件发生
            if (--ready_number <= 0)
                continue;
        }

        // 接下来的循环处理是查看 event_set 里面其他的事件,
        // 也就是已连接套接字的可读事件
        // 这是通过遍历 event_set 数组来完成的
        for (i = 1; i < INIT_SIZE; i++)
        {
            int socket_fd;
            // 如果数组里的 pollfd 的 fd 为 -1，说明这个 pollfd 没有递交有效的检测，直接跳过
            if ((socket_fd = event_set[i].fd) < 0)
                continue;

            // 通过检测 revents 的事件类型是 POLLRDNORM 或者 POLLERR，我们可以进行读操作
            if (event_set[i].revents & (POLLRDNORM | POLLERR))
            {
                if ((n = read(socket_fd, buf, MAXLINE)) > 0)
                {
                    if (write(socket_fd, buf, n) < 0)
                    {
                        error(1, errno, "write error");
                    }
                }
                else if (n == 0 || errno == ECONNRESET)
                {
                    close(socket_fd);
                    event_set[i].fd = -1;
                }
                else
                {
                    error(1, errno, "read error");
                }

                // 和前面的优化加速处理一样
                // 判断如果事件已经被完全处理完之后
                // 直接跳过对 event_set 的循环处理
                // 再次来到 poll 调用
                if (--ready_number <= 0)
                    break;
            }
        }
    }
}

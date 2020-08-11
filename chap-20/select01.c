#include "lib/common.h"

#define MAXLINE 1024
/*
我一直很好奇，为啥说select函数对fd有1024的限制，找了点资料共勉：
首先，man select，搜索FD_SETSIZE会看到如下的内容
An fd_set is a fixed size buffer. 
Executing FD_CLR() or FD_SET() with a value of fd 
that is negative or is equal to or larger than FD_SETSIZE will result in undefined behavior. 
Moreover, POSIX requires fd to be a valid file descriptor.
其中最关键的是FD_SETSIZE，是在bitmap位图运算的时候会受到他的影响
其次，sys/select.h头文件有如下定义：
#define FD_SETSIZE __FD_SETSIZE
typesizes.h头文件有如下定义：
#define __FD_SETSIZE 1024

由此，终于看到了1024的准确限制。

同时man里也说明了一个限制，不是0-1023的fd会导致未定义的行为。
*/

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        error(1, 0, "usage: select01 <IPaddress>");
    }
    int socket_fd = tcp_client(argv[1], SERV_PORT);

    char recv_line[MAXLINE], send_line[MAXLINE];
    int n;

    fd_set readmask;
    fd_set allreads;

    // FD_ZERO 用来将这个向量的所有元素都设置成 0
    // allreads可以看作是一个数组, 每个元素只能为0, 1, 即位图
    FD_ZERO(&allreads);

    // FD_SET 用来把对应套接字 fd 的元素，a[fd]设置成 1
    // 把标准输入0和连接套接字3设置为1
    FD_SET(0, &allreads);
    FD_SET(socket_fd, &allreads);

    for (;;)
    {
        // 注意
        // 每次测试完之后，重新设置待测试的描述符集合
        // 因为 select 调用每次完成测试之后，内核都会修改描述符集合，
        // 通过修改完的描述符集合来和应用程序交互，
        // 应用程序使用 FD_ISSET 来对每个描述符进行判断，从而知道什么样的事件发生
        readmask = allreads;
        // int select(int maxfd, fd_set *readset, fd_set *writeset, fd_set *exceptset, const struct timeval *timeout);
        // 返回：若有就绪描述符则为其数目，若超时则为0，若出错则为 - 1
        // timeout为NULL, 表示如果没有 I/O 事件发生，则 select 一直等待下去
        // 使用 socket_fd+1 来表示待测试的描述符基数。切记需要 +1
        int rc = select(socket_fd + 1, &readmask, NULL, NULL, NULL);

        if (rc <= 0)
        {
            error(1, errno, "select failed");
        }

        // FD_ISSET 对这个向量进行检测，判断出对应套接字的元素 a[fd]是 0 还是 1
        // 其中 0 代表不需要处理，1 代表需要处理

        // 如果是连接描述字准备好可读了,
        // 使用 read 将套接字数据读出
        if (FD_ISSET(socket_fd, &readmask))
        {
            n = read(socket_fd, recv_line, MAXLINE);
            if (n < 0)
            {
                error(1, errno, "read error");
            }
            else if (n == 0)
            {
                error(1, 0, "server terminated \n");
            }
            recv_line[n] = 0;
            fputs(recv_line, stdout);
            fputs("\n", stdout);
        }

        // 标准输入0可读, 程序读入后发送给对端
        if (FD_ISSET(STDIN_FILENO, &readmask))
        {
            if (fgets(send_line, MAXLINE, stdin) != NULL)
            {
                int i = strlen(send_line);
                if (send_line[i - 1] == '\n')
                {
                    send_line[i - 1] = 0;
                }

                printf("now sending %s\n", send_line);
                ssize_t rt = write(socket_fd, send_line, strlen(send_line));
                if (rt < 0)
                {
                    error(1, errno, "write failed ");
                }
                printf("send bytes: %zu \n", rt);
            }
        }
    }
}

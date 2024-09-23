/**
 * @file name : tcp_client.c
 * @brief     : 从终端输入服务器IP 端口号  与服务器建立TCP连接 并相互通信
 * @author    : RISE_AND_GRIND@163.com
 * @date      : 2024年6月5日
 * @version   : 1.0
 * @note      : 编译命令 gcc tcp_client.c -o tcp_client.out -pthread
 *              运行:./tcp_client.out 1xx.7x.1x.2xx 50001
 *              输入 exit 退出客户端
 * CopyRight (c)  2023-2024   RISE_AND_GRIND@163.com   All Right Reseverd
 */

#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#define BUF_SIZE 1024 // 缓冲区大小(字节)
/**
 * @name      receive_from_server
 * @brief     接收线程函数, 用于处理C-->S的信息
 * @param     arg 线程例程参数, 传入服务器的网络信息
 * @note
 */
void *receive_from_server(void *arg)
{
    int tcp_socket_fd = *(int *)arg;
    char buf[BUF_SIZE];
    while (true)
    {
        memset(buf, 0, sizeof(buf));
        ssize_t bytes_received = recv(tcp_socket_fd, buf, sizeof(buf) - 1, 0);
        // puts("");
        if (bytes_received > 0)
        {
            printf("从服务器接收到数据: %s\n", buf);
        }
        // else if (bytes_received == 0)
        // {
        //     printf("服务器断开连接\n");
        //     break;
        // }
        // else
        // {
        //     perror("接收错误");
        //     break;
        // }
        else
        {
            if(bytes_received == 0) printf("服务器断开连接\n");
            else perror("接受错误");
            close(tcp_socket_fd);   //客户端也要释放套接字，避免进入CLOSE_WAIT状态
            break;
        }
    }
    return NULL;
}
// 运行客户端可执行文件 ./xxx 目标服务器地址 服务器端口
int main(int argc, char const *argv[])
{
    // 检查参数有效性
    if (argc != 3)
    {
        fprintf(stderr, "从终端输入的参数无效, errno:%d,%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /************第一步: 打开套接字, 得到套接字描述符************/
    int tcp_socket_fd = socket(AF_INET, SOCK_STREAM, 0);    //ipv4 tcp 默认
    if (tcp_socket_fd < 0)
    {
        fprintf(stderr, "tcp socket error,errno:%d,%s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    /************第一步END*************/

    /************第二步: 调用connect连接远端服务器************/
    struct sockaddr_in server_addr; // 服务器IP信息结构体
    memset(&server_addr, 0, sizeof(server_addr));
    
    // 配置服务器信息结构体
    server_addr.sin_family = AF_INET;                 // 协议族，是固定的
    server_addr.sin_port = htons(atoi(argv[2]));      // 服务器端口，必须转换为网络字节序
    server_addr.sin_addr.s_addr = inet_addr(argv[1]); // 服务器地址 "192.168.64.xxx"
    
    int ret = connect(tcp_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("连接错误:");
        close(tcp_socket_fd);
        exit(EXIT_FAILURE);
    }
    printf("服务器连接成功...\n\n");
    /************第二步END*************/

    /************第三步: 向服务器发送数据************/

    // 创建接收线程
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_from_server, &tcp_socket_fd) != 0)
    {
        perror("线程创建失败");
        close(tcp_socket_fd);
        exit(EXIT_FAILURE);
    }
    pthread_detach(recv_thread); // 线程分离 主要目的是使得线程在终止时能够自动释放其占用的资源，而不需要其他线程显式地调用 pthread_join 来清理它。
    
    /* 向服务器发送数据 */
    char buf[BUF_SIZE]; // 数据收发缓冲区
    while(true)
    {
        // 清理缓冲区
        memset(buf, 0, sizeof(buf));
        // 接收客户端输入的字符串数据
        printf("请输入要发送的字符串: ");
        
        if (fgets(buf, sizeof(buf), stdin) == NULL)
        {
            perror("fgets error");
            break;
        }

        // 将用户输入的数据发送给服务器
        ret = send(tcp_socket_fd, buf, strlen(buf), 0);
        if (ret < 0)
        {
            perror("发送错误:");
            break;
        }

        // 输入了"exit"，退出循环
        if (strncmp(buf, "exit", 4) == 0)
            break;
    }
    /************第三步END*************/
    close(tcp_socket_fd);
    printf("客户端程序结束\n");
    exit(EXIT_SUCCESS);

    return 0;
}

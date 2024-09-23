/**
 * @file name : tcp_server.c
 * @brief     : TCP服务器IP, 响应客户端 端口号  与客户端建立链接
 * @author    : RISE_AND_GRIND@163.com
 * @date      : 2024年6月5日
 * @version   : 1.0
 * @note      : 编译命令 gcc tcp_server.c -o tcp_server.out -pthread
 * // 运行服务器可执行文件 ./xxx 要监听的端口
 *              运行:./tcp_server.out  50001
 *              输入 exit 退出服务器端
 * 待解决: 当客户端结束后, 服务器端需要发送一个任意信息(无效), 接通下一个客户端
 * CopyRight (c)  2023-2024   RISE_AND_GRIND@163.com   All Right Reseverd
 */
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define BUF_SIZE 1024 // 缓冲区大小(字节)

// 客户端网路信息结构体
typedef struct
{
    int sock_fd;                      // 套接字文件描述符
    struct sockaddr_in socket_addr;   // 定义套接字所需的地址信息结构体
    socklen_t addr_len;               // 目标地址的长度
    char receive_msgBuffer[BUF_SIZE]; // 发送给客户端的保活包, 表示我是服务器, C<---NET--S
} ClientArgs_t;

int client_live_flag = -1;      //客户端状态：-1默认 0表示退出 1表示在线
volatile sig_atomic_t stop = 0; // volatile类型表示随时可变的信号量

// 信号处理程序，当接收到 SIGINT 信号时（通常是按下 Ctrl+C），它将 stop 变量设置为 1。
void handle_sigint(int sig)
{
    stop = 1;
}

/**
 * @name      ReceivedFromClient
 * @brief     接收线程函数, 用于处理C-->S的信息
 * @param     client_args 线程例程参数, 传入保活包的网络信息
 * @note
 */

void *ReceivedFromClient(void *client_args)
{
    // 用于传入的是void* 需要强转才能正确指向
    ClientArgs_t *ka_client_args = (ClientArgs_t *)client_args;
    while (!stop) // 信号处理，以便更优雅地退出程序。
    {
        ssize_t bytes_read = read(ka_client_args->sock_fd, ka_client_args->receive_msgBuffer, sizeof(ka_client_args->receive_msgBuffer));
        if (bytes_read > 0)
        {
            //网络字节流是二进制，因此要转换成十进制inet_ntoa()。
            printf("recv from [%s], data is = %s\n", inet_ntoa(ka_client_args->socket_addr.sin_addr), ka_client_args->receive_msgBuffer);
            //缓冲区清空
            bzero(ka_client_args->receive_msgBuffer, sizeof(ka_client_args->receive_msgBuffer));
        }
        else if (bytes_read == 0)
        {
            printf("客户端断开连接\n");
            break;
        }
        else
        {
            perror("读取客户端发送的数据错误");
            break;
        }
    }
    close(ka_client_args->sock_fd); // 关闭与该客户端的链接
    free(ka_client_args);           // 释放空间
    client_live_flag = 0;           // 客户端退出
    pthread_exit(NULL);             // 退出子线程
}

int main(int argc, char const *argv[])
{

    // 检查参数有效性
    if (argc != 2)
    {
        fprintf(stderr, "请正确输入端口号: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, handle_sigint); // 捕捉信号
    
	/************第一步: 打开套接字, 得到套接字描述符************/
    // 1.创建TCP套接字
    int tcp_socket = 0, connect_fd = 0;
	
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);   //参数含义：ipv4 tcp
    if (tcp_socket == -1)
    {
        fprintf(stderr, "tcp套接字打开错误, errno:%d, %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    /************END*************/

    /************第二步: 将套接字描述符与端口绑定************/

    // 生成服务器端的ip和port等
    struct sockaddr_in server_addr;     //sockaddr_in和sockaddr都是结构体，前者将port和addr分开
    // 配置服务器地址信息 接受来自任何地方的数据 包有效 但只解析输入端口范围的包
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                // 协议族，是固定的
    // 目标地址 INADDR_ANY 这个宏是一个整数0.0.0.0，表示所有地址，所以需要使用htonl转换为网络字节序
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_addr.sin_port = htons(atoi(argv[1]));     // 服务器端口，必须转换为网络字节序

    // 绑定socket到指定端口
    if (bind(tcp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "将服务器套接字文件描述符绑定IP失败, errno:%d, %s\n", errno, strerror(errno));
        close(tcp_socket);
        exit(EXIT_FAILURE);
    }
    /************END*************/

    /************第三步: 设置监听信息************/
    // 3.设置监听  队列最大容量是5
    if (listen(tcp_socket, 5) < 0)
    {
        fprintf(stderr, "设置监听失败, errno:%d, %s\n", errno, strerror(errno));
        close(tcp_socket);
        exit(EXIT_FAILURE);
    }
    printf("服务器已经运行, 开始监听中...\n");
    /************END*************/
	
	fd_set read_fd;
	//STDIN_FILENO是标准输入的文件描述名，值为0
	int max_fd = tcp_socket > STDIN_FILENO ? tcp_socket : STDIN_FILENO;
	
    while (!stop)
    {
        /************第四步: 等待连接************/
        // 4.等待接受客户端的连接请求, 阻塞等待有一个C请求连接
        //让程序同时监听两个事件，一个是标准输入，一个是accept函数
			
		printf("从队列出取出一个请求或等待新的客户端连接\n");
		struct sockaddr_in client;
		socklen_t client_len = sizeof(client);
		
		FD_ZERO(&read_fd);
		FD_SET(tcp_socket, &read_fd);
		FD_SET(STDIN_FILENO, &read_fd);
		
		//利用select来分别监听标准输入和连接请求
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) < 0 && errno != EINTR) {
            perror("select 出错");
            break;
        }
		
		if(FD_ISSET(STDIN_FILENO, &read_fd))
		{
			char command[BUF_SIZE];
            fgets(command, sizeof(command), stdin);
            if (strncmp(command, "exit", 4) == 0) {
                break;
            }
			else continue;
		}
		
		if(FD_ISSET(tcp_socket, &read_fd))
		{
			connect_fd = accept(tcp_socket, (struct sockaddr *)&client, &client_len); // 会阻塞
        	if (connect_fd < 0)
        	{
            	if (errno == EINTR) continue;
            	perror("接受连接失败, 队列异常");
            	continue;
        	}
        	client_live_flag = 1;
        	printf("已经从队列出取出一个请求, 连接成功\n");
			FD_CLR(STDIN_FILENO, &read_fd);
		}

        // 此时得到该客户端的新套接字, 使用子线程用于接收客户端信息, 主线程发送信息给客户端
        
        /************END*************/

        /*********************创建接收线程********************/
        // 子线程专属 客户端信息结构体
        ClientArgs_t *client_args = (ClientArgs_t *)malloc(sizeof(ClientArgs_t));
        if (client_args == NULL)
        {
            fprintf(stderr, "线程专属 客户端信息结构体 内存分配失败\n");
            close(connect_fd);
            continue;
        }

        // 配置客户端信息结构体, 将信息传递到子线程
        client_args->addr_len = client_len;
        client_args->sock_fd = connect_fd;
        client_args->socket_addr = client;
        memset(client_args->receive_msgBuffer, 0, BUF_SIZE);

        pthread_t ReceivedFromClient_thread; // 用于接收客户端传回的信息 新线程的TID
        // 创建接收线程  并将客户端IP信息结构体信息传入线程
        if (pthread_create(&ReceivedFromClient_thread, NULL, ReceivedFromClient, (void *)client_args) != 0)
        {
            fprintf(stderr, "创建接收线程错误, errno:%d, %s\n", errno, strerror(errno));
            close(connect_fd); // 关闭对客户端套接字
            free(client_args);
            continue; // 进入下一个请求
        }
        pthread_detach(ReceivedFromClient_thread); // 线程分离 主要目的是使得线程在终止时能够自动释放其占用的资源，而不需要其他线程显式地调用 pthread_join 来清理它。
        /************END*************/

        /************第五步: 主线程发送数据给客户端************/
        char buffer[BUF_SIZE]; // 存放要发的数据缓冲区
        while (!stop)
        {
            if (client_live_flag == 0) break;
            // 清理缓冲区
            memset(buffer, 0x0, sizeof(buffer));
            // 接收用户输入的字符串数据
            printf("请输入要发送的字符串(输入exit退出服务器程序):");
            fgets(buffer, sizeof(buffer), stdin);
            // 输入了"exit"，退出循环
            if (strncmp(buffer, "exit", 4) == 0)    //比较buffer和exit的前4个字符是否相同
            {
                close(connect_fd);
                printf("服务器端关闭连接\n");
                break;
            }
            // 将用户输入的数据发送给服务器
            if (send(connect_fd, buffer, strlen(buffer), 0) < 0)
            {
                perror("发送错误:");
                break;
            }
        }
    }
    close(tcp_socket);
    printf("服务器程序结束\n");
    
    return 0;
}

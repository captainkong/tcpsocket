#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

//==========================================================================
// 语法格式：	int main(int argc, char *argv[])
// 实现功能：	主函数，建立一个TCP_echo_client
// 入口参数：	可以传参改变服务器的ip地址和其端口号，亦可不传参使用默认值
// 出口参数：	无
//==========================================================================

int main(int argc, char *argv[])
{
	unsigned short port = 8000;        // 服务器的端口号
	char *server_ip = "192.168.222.183";      // 服务器ip地址
	char send_buf[512] = "hello every!";	
	char recv_buf[512] = "";
	int sockfd = 0;
	int err_log = 0;
	struct sockaddr_in server_addr;

	if( argc > 1 )		//函数传参，可以更改服务器的ip地址									
	{		
		server_ip = argv[1];
	}	
	
	if( argc > 2 )	   //函数传参，可以更改服务器的端口号									
	{
		port = atoi(argv[2]);
	}

	bzero(&server_addr,sizeof(server_addr));     // 初始化服务器地址
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);        // 创建通信端点：套接字
	if(sockfd < 0)
	{
		perror("socket");
		exit(-1);
	}
	
	err_log = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));      // 主动连接服务器
	if(err_log != 0)
	{
		perror("connect");
		close(sockfd);
		exit(-1);
	}
	
	send(sockfd, send_buf, strlen(send_buf), 0);   // 向服务器发送信息
	recv(sockfd, recv_buf, sizeof(recv_buf), 0);   // 接收服务器发回的信息
	printf("%s\n", recv_buf);
	
	close(sockfd);
	return 0;
}

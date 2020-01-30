/*=============================================================
 文件名称：tcp_echo_srv.c
 功能描述：TCP Echo Server  循环接收TCP发送来的数据,并将接收到的数据回发
 维护记录：2008-12-25  V1.0
           2011-08-11  v1.1
		   2013-02-19  v1.2
 使用方法: ./tcp_echo_srv 8001(或者 ./tcp_echo_srv)
 telnet客户端关闭方法: 第一步: ctrl+]  第二步:quit
===============================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>						
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>				

//=============================================================
// 语法格式：	int main(int argc, char *argv[])
// 实现功能：	主函数，建立一个TCP Echo Server
// 入口参数：	可以传参改变自己的端口号，亦可不传参使用默认值
// 出口参数：	无
//=============================================================

int main(int argc, char *argv[])
{
	char recv_buf[2048] = "";			// 接收缓冲区
	int sockfd = 0;						// 套接字
	int connfd = 0;
	int err_log = 0;
	struct sockaddr_in my_addr;		// 服务器地址结构体
	unsigned short port = 8000;			// 监听端口	
	
	if(argc > 1)						// 由参数接收端口
	{
		port = atoi(argv[1]);
	}
	
	printf("TCP Server Started at port %d!\n", port);
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);   // 创建TCP套接字
	if(sockfd < 0)
	{
		perror("socket");
		exit(-1);
	}
	
	bzero(&my_addr, sizeof(my_addr));	     // 初始化服务器地址
	my_addr.sin_family = AF_INET;
	my_addr.sin_port   = htons(port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	printf("Binding server to port %d\n", port);
	
	err_log = bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if( err_log != 0)
	{
		perror("binding");
		close(sockfd);		
		exit(-1);
	}
	
	err_log = listen(sockfd, 10);
	if(err_log != 0)
	{
		perror("listen");
		close(sockfd);		
		exit(-1);
	}	
	
	printf("Waiting client...\n");
	
	while(1)
	{
		size_t recv_len = 0;
		struct sockaddr_in client_addr;		   // 用于保存客户端地址
		char cli_ip[INET_ADDRSTRLEN] = "";	    // 用于保存客户端IP地址
		socklen_t cliaddr_len = sizeof(client_addr);      // 必须初始化!!!

		connfd = accept(sockfd, (struct sockaddr*)&client_addr, &cliaddr_len);       // 获得一个已经建立的连接
		if(connfd < 0)
		{
			perror("accept");
			continue;
		}
		
		inet_ntop(AF_INET, &client_addr.sin_addr, cli_ip, INET_ADDRSTRLEN);
		printf("client ip = %s\n", cli_ip);
		
		while((recv_len  = recv(connfd, recv_buf, sizeof(recv_buf), 0)) > 0)
		{
			send(connfd, recv_buf, recv_len, 0); 
		}
		
		close(connfd);     //关闭已连接套接字
		printf("client closed!\n");
	}
	
	close(sockfd);         //关闭监听套接字
	return 0;
}

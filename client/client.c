#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#define MAX_MEM_COUNT 20  //内存中最大聊天记录个数
#define MAX_LIST_COUNT 10 //客户端显示的最大聊天记录个数
#define MAXLINE 1024


typedef struct _chat_data
{
	char time[10];
	char data[200];
	struct _chat_data *next;
	struct _chat_data *pre;
} chat;

typedef struct combine
{
	int sockfd;
	char filePath[128];
} scom;

chat head;
void *threadsend(void *vargp);
void *threadrecv(void *vargp);

int main(int argc, char *argv[])
{
	unsigned short port = 8000;		// 服务器的端口号
	char *server_ip = "127.0.0.1";	// 服务器ip地址

	char name[10]="Siri";
	char temp[10];
	int sockfd = 0;
	int err_log = 0;
	struct sockaddr_in server_addr;
	head.next = NULL;
	head.pre = NULL;

	//printf("请输入你的姓名:\n");
	//scanf("%s", &name);
	

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建通信端点：套接字
	if (sockfd < 0)
	{
		perror("socket");
		exit(-1);
	}

	err_log = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)); // 主动连接服务器
	if (err_log != 0)
	{
		perror("connect");
		close(sockfd);
		exit(-1);
	}

	sprintf(temp, "_._%s", name);

	//向服务端发送个人信息
	send(sockfd, temp, strlen(temp), 0); // 向服务器发送信息

	pthread_t tid1, tid2;
	pthread_create(&tid1, NULL, threadsend, &sockfd);
	pthread_create(&tid2, NULL, threadrecv, &sockfd);
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	close(sockfd);
	return 0;
}


void *threadsend(void *vargp)
{
	char tem[512]="this is a test!";
	char send_buf[512];
	int sockfd = *((int *)vargp);
	sprintf(send_buf, ".._%s", tem);	//捆绑请求类型
	send(sockfd, send_buf, strlen(send_buf), 0); // 向服务器发送信息

	/*
	while (1)
	{
		gets(tem);
		if (strlen(tem) == 0)
		{
			continue;
		}
		if (strlen(tem) == 1&&tem[0] == 'q')
		{
			printf("bye...\n");
			exit(0);
		}
		else
		{
			printf("ME:%s\n",tem);
			sprintf(send_buf, "__.%s", tem);
			send(sockfd, send_buf, strlen(send_buf), 0); // 向服务器发送信息
			
		}
		memset(send_buf, 0, sizeof(send_buf));
	}

	*/
	return NULL;
}

void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];

	//while (1)
	//{
		recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
		printf("%s\n",recv_buf );	
		memset(recv_buf, 0, sizeof(recv_buf));
	//}

	return NULL;
}


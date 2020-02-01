/*
	请使用Ctrl+f选择文件发送 Ctrl+g选择文件下载
	captainkong版权所有
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>

#define MAX_MEM_COUNT 20  //内存中最大聊天记录个数
#define MAX_LIST_COUNT 10 //客户端显示的最大聊天记录个数
#define MAXLINE 1024

int bolckCount = 0; //当前数据块个数
sem_t sem;			//信号量 终端使用权

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
void logChat(chat *head);
void *threadsend(void *vargp);
void *threadrecv(void *vargp);
void getTime(char *psTime);
void getDate(char *psDate);
void addToList(char *data);
void uiCls();

int main(int argc, char *argv[])
{
	unsigned short port = 8000;	// 服务器的端口号
	char *server_ip = "127.0.0.1"; // 服务器ip地址
	//char send_buf[512] = "";
	//char recv_buf[512] = "";
	sem_init(&sem, 0, 1);
	char name[10];
	char temp[10];
	int sockfd = 0;
	int err_log = 0;
	struct sockaddr_in server_addr;
	head.next = NULL;
	head.pre = NULL;

	printf("请输入你的姓名:\n");
	scanf("%s", &name);

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

void uiCls()
{
	sem_wait(&sem);
	system("clear");
	chat *tem = &head;

	int count = bolckCount > MAX_LIST_COUNT - 1 ? MAX_LIST_COUNT : bolckCount;
	for (int i = 0; i < count && tem; i++)
	{
		tem = tem->next;
	}
	for (int i = 0; i < count && tem; i++)
	{
		printf("%s\n", tem->data);
		tem = tem->pre;
	}
	while (count++ < MAX_LIST_COUNT)
	{
		printf("\n");
	}

	//printf("请发送:");
	sem_post(&sem);
}

void addToList(char *data)
{
	chat *tem;
	tem = (chat *)malloc(sizeof(chat));
	if (head.next)
		head.next->pre = tem;
	tem->next = head.next;
	head.next = tem;
	tem->pre = &head;
	getTime(tem->time);
	strcpy(tem->data, data);

	if (++bolckCount == MAX_MEM_COUNT)
	{
		printf("即将记录日志\n");
		logChat(&head);
		bolckCount = MAX_LIST_COUNT;
	}
}

void *threadsend(void *vargp)
{
	char tem[512];
	char send_buf[512];
	char filePath[128];
	int sockfd = *((int *)vargp);

	//uiCls();
	getchar();
	while (1)
	{
		gets(tem);
		if (strlen(tem) == 0)
		{
			//uiCls();
			continue;
		}
		if (strlen(tem) == 1&&tem[0] == 'q')
		{
			printf("bye...\n");
			exit(0);
		}
		else
		{
			sprintf(send_buf, "__.%s", tem);
			send(sockfd, send_buf, strlen(send_buf), 0); // 向服务器发送信息
			sprintf(send_buf, "ME:%s", tem);
			//addToList(send_buf);
			//uiCls();
		}
		memset(send_buf, 0, sizeof(send_buf));
	}
}

void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];

	while (1)
	{
		recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
		printf("%s\n",recv_buf );
		//addToList(recv_buf);
		//uiCls();
		
		memset(recv_buf, 0, sizeof(recv_buf));
	}

	return NULL;
}

void getDate(char *psDate)
{
	time_t nSeconds;
	struct tm *pTM;

	time(&nSeconds); // 同 nSeconds = time(NULL);
	pTM = localtime(&nSeconds);

	/* 系统日期,格式:YYYMMDD */
	sprintf(psDate, "%04d-%02d-%02d",
			pTM->tm_year + 1900, pTM->tm_mon + 1, pTM->tm_mday);
}

void getTime(char *psTime)
{
	time_t nSeconds;
	struct tm *pTM;

	time(&nSeconds);
	pTM = localtime(&nSeconds);

	/* 系统时间，格式: HHMMSS */
	sprintf(psTime, "%02d:%02d:%02d",
			pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
}

void logChat(chat *head)
{
	chat *finder = head;
	chat *temp, *tem;
	//找到开始持久化的节点
	for (int i = 0; i < MAX_LIST_COUNT; i++)
	{ //&&finder
		finder = finder->next;
	}
	temp = finder->next;
	finder->next = NULL;
	temp->pre = NULL;

	//将指针定位到链表尾部
	while (temp->next)
	{
		temp = temp->next;
	}

	FILE *fp;
	if ((fp = fopen("log.txt", "a+")) == NULL)
	{
		//printf("文件开始写入\n");
	}

	while (temp)
	{
		tem = temp;
		if (tem->pre)
			tem->pre->next = NULL;
		temp = temp->pre;
		fprintf(fp, "%s - %s\n", tem->time, tem->data);
		free(tem);
	}

	fclose(fp);
}

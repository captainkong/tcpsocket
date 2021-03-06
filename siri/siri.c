/**
 * 日期:	2020年2月3日
 * 文件名:	siri.c
 * 功能:	响应Siri调用请求
 * 入口:	siri.php网页调用
 * 参数:	siri 调用命令
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
#include "../lib/cJSON/cJSON.h"


void *threadsend(void *vargp);
void *threadrecv(void *vargp);

char arg[64];	//存贮参数

int main(int argc, char *argv[])
{
	if(argc>1) strcpy(arg,argv[1]);

	unsigned short port = 8000;		// 服务器的端口号
	char *server_ip = "127.0.0.1";	// 服务器ip地址
	//char name[10]="Siri";
	int sockfd = 0;
	int err_log = 0;
	struct sockaddr_in server_addr;
	
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

	cJSON* root=cJSON_CreateObject();	
	cJSON_AddStringToObject(root, "type","SI_CONNECT");
	cJSON_AddStringToObject(root, "data","siri");
    char * out=cJSON_Print(root);
	cJSON_Delete(root);
	//printf("发送:%s\n",out);
	send(sockfd, out, strlen(out), 0); // 向服务器发送信息
	free(out);

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
	int sockfd = *((int *)vargp);
	
	cJSON* root=cJSON_CreateObject();	
	cJSON_AddStringToObject(root, "type","S_CON");
	cJSON_AddStringToObject(root, "data",arg);
	
    char * out=cJSON_Print(root);
	cJSON_Delete(root);
	//printf("%s\n",out);
	 
	send(sockfd, out, strlen(out), 0); // 向服务器发送信息
	free(out);
	if(0==strcmp("G_DATA",arg)||0==strcmp("S_DATA",arg))
	{
		close(sockfd);
		exit(0);
	}
		
	
	return NULL;
}

void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];
	cJSON *json,*item;
	for(int i=0;i<2;i++)
	{
		memset(recv_buf, 0, sizeof(recv_buf));
		recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
		//printf("%s\n",recv_buf );
		json = cJSON_Parse((const char *)recv_buf);
		if (!json)
		{
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		}
		item = cJSON_GetObjectItem(json, "type");
		if (!item)
		{
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		}
		if(0==strcmp(item->valuestring,"CON_R"))
		{
			item = cJSON_GetObjectItem(json, "data");
			if (!item)
			{
				printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			}
			//printf("1%s\n",item->valuestring);
			if(0==strcmp(item->valuestring,"error")){
				cJSON* root=cJSON_CreateObject();	
				cJSON_AddStringToObject(root, "type","S_CON");
				cJSON_AddStringToObject(root, "data","arg");
				
				char * out=cJSON_Print(root);
				cJSON_Delete(root);
				//printf("%s\n",out);
				
				send(sockfd, out, strlen(out), 0); // 向服务器发送信息
				free(out);
				printf("发生错误,主动断开!\n");
				exit(1);
			}else if(0==strcmp(item->valuestring,"ok"))
			{
				//printf("连接到服务器...\n");
			}else{
				printf("%s\n",item->valuestring);
			}
	}

	}
	
	
	
	return NULL;
}


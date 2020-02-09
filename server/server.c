#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "../lib/cJSON/cJSON.h"
#include "../lib/openssl/operate_aes.h"  

#define PRIVATEKEY 			"rsa_private_key.pem"
#define CLIENT_MAX_COUNT	5	//最大连接数量
#define	USER_LENGTH_KEY		65	//aes密钥长度(十六进制字符串)
#define	USER_LENGTH_NAME	65	//设备名长度


//服务请求类型
#define CONNECT 1 //请求连接
#define NORMAL	2  //聊天内容
#define S_CON 	3   //SIRI控制请求
#define TEST 	4	//请求下载文件

typedef struct _client_type			//设备结构体
{
	char ip[INET_ADDRSTRLEN]; 		//字符串ip
	int socket;				 		//套接字
	char nickNme[USER_LENGTH_KEY];	//设备名
	char aes_key[USER_LENGTH_NAME];	//加密密钥(十六进制,注意长度)
} client;

void *threadConnect(void *vargp);
void sayHello(char *str, char *ip, int index);
void broadcast(char *str, int except);
void initRsa();
int getClientCunt();
int getIndexBySocket(int socket);
int getFreeIndex();
int getRequestType(char *str);



/*      全局变量    */
client clientList[CLIENT_MAX_COUNT];
RSA *privateRsa = NULL;


int main(int argc, char *argv[])
{
	//char recv_buf[2048] = ""; // 接收缓冲区
	int sockfd = 0;			  // 套接字
	int connfd = 0;
	int err_log = 0;
	struct sockaddr_in my_addr; // 服务器地址结构体
	unsigned short port = 8000; // 监听端口

	initRsa();	//初始化rsa私钥

	printf("TCP Server Started at port %d!\n", port);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建TCP套接字
	if (sockfd < 0)
	{
		perror("socket");
		exit(-1);
	}

	//bzero(&my_addr, sizeof(my_addr));	     // 初始化服务器地址
	memset(&my_addr, 0, sizeof(my_addr));

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	printf("Binding server to port %d\n", port);

	err_log = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
	if (err_log != 0)
	{
		perror("binding");
		close(sockfd);
		exit(-1);
	}

	err_log = listen(sockfd, 10);
	if (err_log != 0)
	{
		perror("listen");
		close(sockfd);
		exit(-1);
	}

	printf("Waiting client...\n");

	while (1)
	{
		//size_t recv_len = 0;
		struct sockaddr_in client_addr;				 // 用于保存客户端地址
		socklen_t cliaddr_len = sizeof(client_addr); // 必须初始化!!!

		connfd = accept(sockfd, (struct sockaddr *)&client_addr, &cliaddr_len); // 获得一个已经建立的连接
		if (connfd < 0)
		{
			perror("accept");
			continue;
		}

		int index = getFreeIndex();
		if (index == -1)
		{
			printf("达到设定阈值!\n");
			break;
		}

		inet_ntop(AF_INET, &client_addr.sin_addr, clientList[index].ip, INET_ADDRSTRLEN);
		//printf("开始在%d设置socket\n",index);
		clientList[index].socket = connfd;
		pthread_t tid;
		pthread_create(&tid, NULL, threadConnect, &connfd);
	}
	RSA_free(privateRsa);
	close(sockfd); //关闭监听套接字
	return 0;
}

void *threadConnect(void *vargp)
{

	size_t recv_len = 0;
	char recv_buf[512]; // 接收缓冲区
	char *encrypt_buf;
	char decrypt_buf[KEY_LENGTH];
	char tem[2048];
	char * out;
	int connfd = *((int *)vargp);
	int index = getIndexBySocket(connfd);
	memset(recv_buf, 0, sizeof(recv_buf));
	cJSON *json,*item;	//用完free

	//接收连接信息	本消息使用公钥加密
	recv_len = recv(connfd, (unsigned char *)recv_buf, sizeof(recv_buf), 0);
	//printf("收到连接消息:%s:\n",recv_buf);
	int rsa_len = RSA_size(privateRsa);
	unsigned char *decryptMsg = (unsigned char *)malloc(rsa_len);
	memset(decryptMsg, 0, rsa_len);
	//printf("rsa_len:%d\n",rsa_len );
	//解密消息
	int mun =  RSA_private_decrypt(rsa_len, (const unsigned char *)recv_buf, decryptMsg, privateRsa, RSA_PKCS1_PADDING);
	memset(recv_buf, 0, sizeof(recv_buf));
	if ( mun < 0)
		printf("!!!!!!!!!!RSA_private_decrypt error\n");
	else
		//printf("RSA_private_decrypt: %s\n", decryptMsg);
	//解析json
	json = cJSON_Parse((const char *)decryptMsg);
	if (!json)
	{
		printf("Error before: [%s]\n", cJSON_GetErrorPtr());
	}
	item = cJSON_GetObjectItem(json, "type");
	if (!item)
	{
		printf("Error before: [%s]\n", cJSON_GetErrorPtr());
	}
	int type = getRequestType(item->valuestring);
	if(type==CONNECT)
	{
		item = cJSON_GetObjectItem(json, "data");
		if (!item)
		{
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		}

		memset(clientList[index].nickNme,0,USER_LENGTH_NAME);
		memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
		strncpy(clientList[index].nickNme, item->valuestring,5);	//暂用密码当做设备名
		strcpy(clientList[index].aes_key, item->valuestring);

		json=cJSON_CreateObject();	
		cJSON_AddStringToObject(json, "type","CON_R");
		cJSON_AddStringToObject(json, "data","ok");
		
		out=cJSON_Print(json);
		//aes_encrypt(out, clientList[index].aes_key, encrypt_buf);
		encrypt_buf=getRightEncrypt(out,clientList[index].aes_key);
		printf("加密结果：\n%s\n", encrypt_buf);
		send(clientList[index].socket, encrypt_buf, KEY_LENGTH, 0);
		free(out);
		free(encrypt_buf);
		out=NULL;
		encrypt_buf=NULL; 
		

		if(0!=strcmp("Siri",clientList[index].nickNme))
		{
			printf("新的连接:%s\n", clientList[index].nickNme); //,clientList[index].socket
			printf("当前在线数:%d\n", getClientCunt());
		}
	}else{
		printf("error in connect type\n");
	}
	


	//从客户端接收数据	使用aes加密
	while ((recv_len = recv(connfd, (unsigned char *)recv_buf, sizeof(recv_buf), 0)) > 0)
	{
		printf("收到消息:%s\n",recv_buf);
		memset(decrypt_buf,0,KEY_LENGTH);
		aes_decrypt(recv_buf, clientList[index].aes_key, decrypt_buf);
        printf("解密结果：%s\n", decrypt_buf);

		//解析json
		json = cJSON_Parse(decrypt_buf);
		if (!json)
		{
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		}
		item = cJSON_GetObjectItem(json, "type");
		if (!item)
		{
			printf("Error before: [%s]\n", cJSON_GetErrorPtr());
		}
		
		//获取请求类型
		int type = getRequestType(item->valuestring);
		switch (type)
		{
		case TEST:
			
			printf("这是一个测试请求!\n");
			break;
		case S_CON:	//Siri控制请求
			item=cJSON_GetObjectItem(json,"data");
			sprintf(tem, "已收到来自Siri的信息:%s",  item->valuestring);
			send(clientList[index].socket, tem, strlen(tem), 0);
			printf("收到来自Siri的信息 : %s\n", item->valuestring);
			
			json=cJSON_CreateObject();	
			cJSON_AddStringToObject(json, "type","S_CON");
			cJSON_AddStringToObject(json, "data",item->valuestring);
			
			out=cJSON_Print(json);
			broadcast(out,index);
			free(out);
			out=NULL;
			break;
		default:
			printf("非法的请求!");
		}
		memset(recv_buf, 0, sizeof(recv_buf));
	}

	close(connfd); 		//关闭已连接套接字
	cJSON_Delete(json);	//清空json
	
	clientList[index].socket = -1;
	if(0!=strcmp("Siri",clientList[index].nickNme))
	{
		printf("%s[%s]离开聊天室!\n", clientList[index].nickNme, clientList[index].ip);
		printf("当前在线数:%d\n", getClientCunt());
	}
	memset(recv_buf, 0, sizeof(recv_buf));
	memset(clientList[index].nickNme, 0, sizeof(clientList[index].nickNme));
	return NULL;
}

//获取请求类型
int getRequestType(char *str)
{
	if(0==strcmp(str,"CONNECT")){		//连接请求
		return CONNECT;
	}else if(0==strcmp(str,"S_CON")){	//Siri控制请求
		return S_CON;
	}else if(0==strcmp(str,"TEST")){
		//SIRI控制请求
		return TEST;
	}
	

	return 0;
}

int getFreeIndex() //从列表中拿到一个可用的地址
{
	for (int i = 0; i < CLIENT_MAX_COUNT; i++)
	{
		//printf("%d\n",clientList[i].socket);
		if (clientList[i].socket<=0)
		{
			return i;
		}
	}
	return -1;
}

int getIndexBySocket(int socket)
{
	for (int i = 0; i < CLIENT_MAX_COUNT; i++)
	{
		if (socket == clientList[i].socket)
		{
			return i;
		}
	}
	return -1;
}

int getClientCunt()
{
	int n = 0;
	for (int i = 0; i < CLIENT_MAX_COUNT; i++)
	{
		if (clientList[i].socket >0)//!= -1
		{
			//printf("位置%d已占用,id:%d\n",i,clientList[i].socket);
			n++;
		}
	}
	return n;
}

//广播消息
void broadcast(char *str, int except)
{
	for (int i = 0; i < CLIENT_MAX_COUNT; i++)
	{
		if (i != except && -1 != clientList[i].socket)
		{
			send(clientList[i].socket, str, strlen(str), 0);
		}
	}
}

//连接时发送欢迎消息
void sayHello(char *str, char *ip, int index)
{
	char name[10];
	char hello[50];
	strcpy(name, str + 3);
	sprintf(hello, "欢迎%s[%s]加入聊天室!", name, ip);
	broadcast(hello, index);
}


void initRsa()
{
	//从文件读入RSA私钥
	FILE *fp = NULL;
	if ((fp = fopen(PRIVATEKEY, "r")) == NULL) 
	{
		printf("private key path error\n");
		exit(1);
	}
	if ((privateRsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) 
	{
		printf("PEM_read_RSAPrivateKey error\n");
		exit(1);
	}
	fclose(fp);
}

/*
		case CONNECT:	//处理树莓派的连接请求
		
			item = cJSON_GetObjectItem(json, "data");
			if (!item)
			{
				printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			}

			memset(clientList[index].nickNme,0,USER_LENGTH_NAME);
			memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
			strcpy(clientList[index].nickNme, item->valuestring);	//暂用密码当做设备名
			strcpy(clientList[index].aes_key, item->valuestring);

			json=cJSON_CreateObject();	
			cJSON_AddStringToObject(json, "type","CON_R");
			cJSON_AddStringToObject(json, "data","ok");
			
			out=cJSON_Print(json);
			aes_encrypt(out, clientList[index].aes_key, encrypt_buf);
    		printf("加密结果：\n%s\n", encrypt_buf);
			send(clientList[index].socket, encrypt_buf, strlen(encrypt_buf), 0);
			free(out);
			out=NULL;
			

			if(0!=strcmp("Siri",clientList[index].nickNme))
			{
				printf("新的连接:%s\n", clientList[index].nickNme); //,clientList[index].socket
				printf("当前在线数:%d\n", getClientCunt());
			}
			break;
			*/

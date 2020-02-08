/**
 * 日期:	2020年2月4日
 * 文件名:	piclient.c
 * 功能:	和服务器通信,完成信息传递和指令下达
 * 入口:	树莓派开机启动(后续开发成守护进程)
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include "../lib/cJSON/cJSON.h"


//请求类型
#define CONNECT 1 	//请求连接
#define NORMAL	2 	//聊天内容
#define S_CON 	3   //SIRI控制请求
#define CLOSE 	4	//关闭连接请求

#define PUBLICKEY "rsa_public_key.pem"
#define PRIVATEKEY "rsa_private_key.pem"


void *threadrecv(void *vargp);
int getRequestType(char *str);	//获取请求类型

char aes_key[130];	//存贮对称密钥

int main(int argc, char *argv[])
{
	unsigned short port = 8000;				// 服务器的端口号
	char *server_ip = "127.0.0.1";	// 服务器ip地址

	char name[10]="PI";
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
	cJSON_AddStringToObject(root, "type","CONNECT");
	cJSON_AddStringToObject(root, "data","这是一个中文测试,希望你能完整看到它");
	
    unsigned char * out=(unsigned char *)cJSON_Print(root);
	cJSON_Delete(root);
	/////////////////// rsa 

	FILE *fp = NULL;
	RSA *publicRsa = NULL;
	RSA *privateRsa = NULL;
	if ((fp = fopen(PUBLICKEY, "r")) == NULL) 
	{
		printf("public key path error\n");
		return -1;
	} 	
   
	if ((publicRsa = PEM_read_RSA_PUBKEY(fp, NULL, NULL, NULL)) == NULL) 
	{
		printf("PEM_read_RSA_PUBKEY error\n");
		return -1;
	}
	fclose(fp);
	
	if ((fp = fopen(PRIVATEKEY, "r")) == NULL) 
	{
		printf("private key path error\n");
		return -1;
	}
	//OpenSSL_add_all_algorithms();//密钥有经过口令加密需要这个函数
	if ((privateRsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL)) == NULL) 
	{
		printf("PEM_read_RSAPrivateKey error\n");
		return NULL;
	}
	fclose(fp);		
	
	//unsigned char *source = (unsigned char *)"{\"type\":\"s_con\"\,\"name\":\"中文测试\"}";
		
	int rsa_len = RSA_size(publicRsa);
	printf("rsa_len:%d\n",rsa_len);
 
	unsigned char *encryptMsg = (unsigned char *)malloc(rsa_len);
	memset(encryptMsg, 0, rsa_len);
 		
	int len = rsa_len - 11;
 		
	if (RSA_public_encrypt(len, out, encryptMsg, publicRsa, RSA_PKCS1_PADDING) < 0)
		printf("RSA_public_encrypt error\n");
	else 
	{
        printf("encoded:%s\n",encryptMsg);
		rsa_len = RSA_size(privateRsa);
		printf("rsa_len(private):%d\n",rsa_len );
		unsigned char *decryptMsg = (unsigned char *)malloc(rsa_len);
		memset(decryptMsg, 0, rsa_len);
	    
		int mun =  RSA_private_decrypt(rsa_len, encryptMsg, decryptMsg, privateRsa, RSA_PKCS1_PADDING);
	 
		if ( mun < 0)
			printf("RSA_private_decrypt error\n");
		else
			printf("RSA_private_decrypt %s\n", decryptMsg);
	}	
	
	RSA_free(publicRsa);
	RSA_free(privateRsa);

	//////////////////
	send(sockfd, encryptMsg, rsa_len, 0); // 向服务器发送信息
	free(out);

	pthread_t tid2;
	//pthread_create(&tid1, NULL, threadsend, &sockfd);
	pthread_create(&tid2, NULL, threadrecv, &sockfd);
	//pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	close(sockfd);
	return 0;
}

void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];
	cJSON *json,*item;	//用完free

	while (1)
	{
		//从服务器接收消息
		memset(recv_buf, 0, sizeof(recv_buf));
		recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
		printf("%s\n",recv_buf );	

		//解析json
		json = cJSON_Parse(recv_buf);
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
		case CONNECT:
			
			break;
		case NORMAL:
			
			break;
		case S_CON:	//Siri控制请求
			printf("this is a si con test!\n");
			break;
		default:
			printf("非法的请求!");
		}
	}

	cJSON_Delete(json);//清空 顺序不能错

	return NULL;
}

//获取请求类型
int getRequestType(char *str)
{
	if(0==strcmp(str,"CONNECT")){		//连接请求
		return CONNECT;
	}else if(0==strcmp(str,"S_CON")){	//Siri控制请求
		return S_CON;
	}else if(0==strcmp(str,".._")){
		//SIRI控制请求
		return S_CON;
	}
	return 0;
}
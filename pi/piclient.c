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
#include "../lib/openssl/operate_aes.h"  


//请求类型
#define CONNECT 1 	//请求连接
#define CON_R	2 	//请求状态返回
#define S_CON 	3   //SIRI控制请求
#define TEST 	4	//关闭连接请求


#define PUBLICKEY 	"rsa_public_key.pem"	//公钥位置
#define PRIVATEKEY	"rsa_private_key.pem"

/*      全局变量    */
char aes_key[65];	//存贮对称密钥

void *threadrecv(void *vargp);
int getRequestType(char *str);

int main(int argc, char *argv[])
{
	unsigned short port = 8000;		// 服务器的端口号
	char *server_ip = "127.0.0.1";	// 服务器ip地址

	//char name[10]="PI";
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

	//生成密钥
    BIGNUM *rnd;
	rnd = BN_new();
	char * key;
	int top =0;
	int bottom = 0;

	BN_rand(rnd,KEY_LENGTH,top,bottom);	
	BN_num_bits(rnd);
	key = BN_bn2hex(rnd);
	strcpy(aes_key,key);
	//printf("length:%d,\nkey:%s.\naes_key:%s\n",length,key,aes_key);
	BN_free(rnd);

	cJSON* root=cJSON_CreateObject();	
	cJSON_AddStringToObject(root, "type","CONNECT");
	cJSON_AddStringToObject(root, "data",key);
	
    unsigned char * out=(unsigned char *)cJSON_Print(root);
	cJSON_Delete(root);

	//初始化rsa
	FILE *fp = NULL;
	RSA *publicRsa = NULL;
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
	
		
	int rsa_len = RSA_size(publicRsa);
 
	unsigned char *encryptMsg = (unsigned char *)malloc(rsa_len);
	memset(encryptMsg, 0, rsa_len);
 		
	int len = rsa_len - 11;
 		
	if (RSA_public_encrypt(len, out, encryptMsg, publicRsa, RSA_PKCS1_PADDING) < 0)
		printf("RSA_public_encrypt error\n");
	
	RSA_free(publicRsa);

	send(sockfd, encryptMsg, rsa_len, 0); // 向服务器发送信息
	free(out);

	pthread_t tid2;
	pthread_create(&tid2, NULL, threadrecv, &sockfd);
	pthread_join(tid2, NULL);

	close(sockfd);
	return 0;
}

void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];
	char decrypt_buf[KEY_LENGTH];
	char *encrypt_buf;
	char * out;
	cJSON *json,*item,*temp;	//用完free
	

	while (1)
	{
		
		memset(recv_buf, 0, sizeof(recv_buf));
		memset(decrypt_buf, 0, KEY_LENGTH);
		//从服务器接收消息
		recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
		printf("收到来自服务器的消息:\n%s\n",recv_buf );
		aes_decrypt(recv_buf, aes_key, decrypt_buf);
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
		case CONNECT:
			
			break;
		case CON_R:
			item = cJSON_GetObjectItem(json, "data");
			if (!item)
			{
				printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			}
			if(0==strcmp("ok",item->valuestring))
			{
				printf("成功建立连接!\n");
			}
			temp=cJSON_CreateObject();	
			cJSON_AddStringToObject(temp, "type","TEST");
			cJSON_AddStringToObject(temp, "data","this is a test!");
			out=cJSON_Print(temp);
			cJSON_Delete(temp);
			encrypt_buf=getRightEncrypt(out,aes_key);
			//aes_encrypt(out, aes_key, encrypt_buf); 
			printf("加密结果：\n%s\n", encrypt_buf);
			send(sockfd, encrypt_buf, KEY_LENGTH, 0); // 向服务器发送信息
			free(out);
			free(encrypt_buf);
			out=NULL;
			encrypt_buf=NULL;
			exit(1);
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
	}else if(0==strcmp(str,"CON_R")){
		//SIRI控制请求
		return CON_R;
	}
	return 0;
}

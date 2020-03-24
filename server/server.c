/**
 * 功能:作为服务端,客户端包括树莓派和同一服务器下的Siri调用程序(响应网页的指令)
*/

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
#include <mysql/mysql.h>
#include "../lib/cJSON/cJSON.h"
#include "../lib/openssl/operate_aes.h"  

#define PRIVATEKEY 			"rsa_private_key.pem"
#define	SIRI_NAME			"Siri响应程序"
#define CLIENT_MAX_COUNT	5	//最大连接数量
#define	USER_LENGTH_KEY		65	//aes密钥长度(十六进制字符串)
#define	USER_LENGTH_NAME	65	//设备名长度
#define	USER_LENGTH_MAC		18	//设备MAC地址长度


//服务请求类型
#define NORMAL		2  	//聊天内容
#define S_CON 		3   //SIRI控制请求
#define TEST		4	//请求下载文件
#define PI_CONNECT	5 	//树莓派请求连接
#define SI_CONNECT	6 	//SIRI响应程序派请求连接P_DATA
#define DATA_ONLE	7	//客户端在线时树莓派发来消息
#define DATA_OFFLE	8	//客户端离线时树莓派发来消息

typedef struct _client_type			//设备结构体
{
	char ip[INET_ADDRSTRLEN]; 		//字符串ip
	int socket;				 		//套接字
	char nickName[USER_LENGTH_KEY];	//设备名
	char macAddr[USER_LENGTH_MAC];	//设备MAC地址
	char aes_key[USER_LENGTH_NAME];	//加密密钥(十六进制,注意长度)
} client;

void *threadConnect(void *vargp);
void sayHello(char *str, char *ip, int index);
void broadcast(char *str, int except);
void initRsa();
int getClientCunt();
int getIndexBySocket(int socket);
int getIndexByMac(char *str);
int getFreeIndex();
int getRequestType(char *str);
int executesql(const char * sql);
int init_mysql();
void print_mysql_error(const char *msg);

//数据库相关
const char *g_host_name = "localhost";
const char *g_user_name = "root";
const char *g_password = "RbzNahIdHG3J2587";
const char *g_db_name = "pi";
const unsigned int g_db_port = 3306;

/*      全局变量    */
client clientList[CLIENT_MAX_COUNT];
RSA *privateRsa = NULL;
MYSQL *sqlCon; // mysql 连接


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

	if (init_mysql())
       print_mysql_error(NULL);
	else
		printf("connect to MySQL\n");

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
	char recv_buf[KEY_LENGTH]; // 接收缓冲区
	char tem[KEY_LENGTH];
	char *encrypt_buf;
	char decrypt_buf[KEY_LENGTH];
	MYSQL_RES *g_res; // mysql 记录集
	MYSQL_ROW g_row; // 字符串数组，mysql 记录行
	
	char * out;
	int connfd = *((int *)vargp);
	int index = getIndexBySocket(connfd);
	memset(recv_buf, 0, sizeof(recv_buf));
	cJSON *json,*item;	//用完free
	memset(recv_buf, 0, sizeof(recv_buf));
	memset(clientList[index].nickName,0,USER_LENGTH_NAME);
	memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
	memset(clientList[index].macAddr,0,USER_LENGTH_MAC);
	

	//接收连接信息	此线程可以用来处理来自树莓派的加密信息,也可以处理来自本机的未加密信息
	//拒绝所有非内网连接的非加密信息
	recv(connfd, (unsigned char *)recv_buf, sizeof(recv_buf), 0);
	//printf("收到%d:%s\n",(int)strlen(recv_buf),recv_buf);
	if(cJSON_Parse((const char *)recv_buf))
	{	//来自本地连接
		if(0!=strcmp(clientList[index].ip,"127.0.0.1"))
		{	//不是来自本地,非法请求
			printf("非法请求!\n");
			json=cJSON_CreateObject();	
			cJSON_AddStringToObject(json, "type","CON_R");
			cJSON_AddStringToObject(json, "data","error");
			out=cJSON_Print(json);
			send(clientList[index].socket, out, strlen(out), 0);
			clientList[index].socket=-1;
			free(out);
			out=NULL;
			//close(connfd); 			//关闭已连接套接字 会崩溃
			cJSON_Delete(json);		//清空json
			return NULL;
		}
		strcpy(clientList[index].nickName, SIRI_NAME);	
		json=cJSON_CreateObject();	
		cJSON_AddStringToObject(json, "type","CON_R");
		cJSON_AddStringToObject(json, "data","ok");
		out=cJSON_Print(json);
		send(clientList[index].socket, out, strlen(out), 0);
		free(out);
		out=NULL;

	}else{
		//来自远程
		int rsa_len = RSA_size(privateRsa);
		unsigned char *decryptMsg = (unsigned char *)malloc(rsa_len);
		memset(decryptMsg, 0, rsa_len);
		//解密消息
		int mun =  RSA_private_decrypt(rsa_len, (const unsigned char *)recv_buf, decryptMsg, privateRsa, RSA_PKCS1_PADDING);
		memset(recv_buf, 0, sizeof(recv_buf));
		if ( mun < 0)
		{
			char str[]="ERROR!\n";
			send(clientList[index].socket, str, strlen(str), 0);
			memset(clientList[index].nickName,0,USER_LENGTH_NAME);
			memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
			memset(clientList[index].macAddr,0,USER_LENGTH_MAC);
			close(connfd);
			printf("异常线程即将退出...\n");
			clientList[index].socket = -1;
			return NULL;
		}
		
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
		if(type==PI_CONNECT)
		{
			item = cJSON_GetObjectItem(json, "data");
			if (!item)
			{
				printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			}

			memset(clientList[index].nickName,0,USER_LENGTH_NAME);
			memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
			memset(clientList[index].macAddr,0,USER_LENGTH_MAC);
			
			strcpy(clientList[index].aes_key, item->valuestring);
			item = cJSON_GetObjectItem(json, "mac");
			if (!item)
			{
				printf("Error before: [%s]\n", cJSON_GetErrorPtr());
			}
			strcpy(clientList[index].macAddr, item->valuestring);
			strcpy(clientList[index].nickName, item->valuestring);	//暂用mac地址当做设备名

			json=cJSON_CreateObject();
			cJSON_AddStringToObject(json, "type","CON_R");
			cJSON_AddStringToObject(json, "data","ok");
			
			out=cJSON_Print(json);
			encrypt_buf=getRightEncrypt(out,clientList[index].aes_key);
			//printf("加密结果：\n%s\n", encrypt_buf);
			send(clientList[index].socket, encrypt_buf, KEY_LENGTH, 0);
			free(out);
			free(encrypt_buf);
			out=NULL;
			encrypt_buf=NULL; 
			

			if(0!=strcmp(SIRI_NAME,clientList[index].nickName))
			{
				printf("新的连接:%s\n", clientList[index].nickName); //,clientList[index].socket
				printf("当前在线数:%d\n", getClientCunt());
			}
		}
		
	}
	memset(recv_buf, 0, sizeof(recv_buf));
	//从客户端接收数据	使用aes加密
	while ((recv(connfd, (unsigned char *)recv_buf, sizeof(recv_buf), 0)) > 0)
	{
		//printf("收到消息:%s\n",recv_buf);
		memset(decrypt_buf,0,KEY_LENGTH);
		if(0!=strcmp(clientList[index].nickName,SIRI_NAME)){
			if(0==aes_decrypt(recv_buf, clientList[index].aes_key, decrypt_buf))
			{
				printf("解密失败!!!!!\n");
			}
			//printf("解密结果：%s\n", decrypt_buf);
			//解析json
			json = cJSON_Parse(decrypt_buf);
		}else{
			//解析json 本地未加密信息
			json = cJSON_Parse(recv_buf);
		}
		
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
		case DATA_ONLE:	//在线时树莓派发送的数据
			//printf("收到数据流:%s\n",decrypt_buf);
			sprintf(tem,"insert into data_online values(NULL,'%s',%d,%d,%d,%d,%d,%d,now())",
			clientList[index].macAddr,
			cJSON_GetObjectItem(json,"pm1.0")->valueint,
			cJSON_GetObjectItem(json,"pm2.5")->valueint,
			cJSON_GetObjectItem(json,"pm10")->valueint,
			cJSON_GetObjectItem(json,"temperature")->valueint,
			cJSON_GetObjectItem(json,"humidity")->valueint,
			cJSON_GetObjectItem(json,"danger")->valueint);
			printf("执行%s\n",tem);
			if (executesql(tem)) // 句末没有分号
        		print_mysql_error(NULL);
			else
				//printf("成功插入一条数据!\n");
			
			break;
		case S_CON:	//Siri控制请求
			item=cJSON_GetObjectItem(json,"data");
			
			printf("收到来自Siri的信息 : %s\n", item->valuestring);
			int macInIndex=getIndexByMac("b8:27:eb:20:48:4b");
			if(macInIndex==-1)
			{
				json=cJSON_CreateObject();
				cJSON_AddStringToObject(json, "type","CON_R");
				cJSON_AddStringToObject(json, "data","NOT ON LINE!");
				out=cJSON_Print(json);
				encrypt_buf=getRightEncrypt(out,clientList[macInIndex].aes_key);
				send(clientList[index].socket, encrypt_buf, strlen(encrypt_buf), 0);
				free(out);
				out=NULL;//退出线程
				memset(clientList[index].nickName,0,USER_LENGTH_NAME);
				memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
				memset(clientList[index].macAddr,0,USER_LENGTH_MAC);
				close(connfd);
				printf("SIRI控制线程即将退出...\n");
				clientList[index].socket = -1;
				return NULL;
			}
			if(0==strcmp("buon",item->valuestring))	//打开蜂鸣器指令
			{//  b8:27:eb:20:48:4b 52:54:00:cc:3b:e4
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","S_CON");
				cJSON_AddStringToObject(json, "data","buon");
				out=cJSON_Print(json);
				encrypt_buf=getRightEncrypt(out,clientList[macInIndex].aes_key);
				send(clientList[macInIndex].socket, encrypt_buf, strlen(encrypt_buf), 0);
				free(out);
				out=NULL;
				//状态返回
				cJSON_Delete(json);	//清空json
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","CON_R");
				cJSON_AddStringToObject(json, "data","已打开蜂鸣器");
				out=cJSON_Print(json);
				send(clientList[index].socket, out, strlen(out), 0);
				printf("已发送给反馈程序%s:\n",out);
				free(out);
				out=NULL;
			}else if(0==strcmp("buoff",item->valuestring))	//关闭蜂鸣器指令
			{//  b8:27:eb:20:48:4b 52:54:00:cc:3b:e4
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","S_CON");
				cJSON_AddStringToObject(json, "data","buoff");
				out=cJSON_Print(json);
				encrypt_buf=getRightEncrypt(out,clientList[macInIndex].aes_key);
				send(clientList[macInIndex].socket, encrypt_buf, strlen(encrypt_buf), 0);
				//printf("....已发送%s\n",encrypt_buf);
				free(out);
				out=NULL;
				//状态返回
				cJSON_Delete(json);	//清空json
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","CON_R");
				cJSON_AddStringToObject(json, "data","已关闭蜂鸣器");
				out=cJSON_Print(json);
				send(clientList[index].socket, out, strlen(out), 0);
				printf("已发送给反馈程序%s:\n",out);
				free(out);
				out=NULL;
			}else if(0==strcmp("get_data",item->valuestring))	//获取数据
			{
				if (executesql("select pm25,round(temperature/10,1),round(humidity/10,1) from data_online order by did desc limit 1")) // 句末没有分号
        			print_mysql_error(NULL);
				g_res = mysql_store_result(sqlCon);
				while ((g_row=mysql_fetch_row(g_res))) // 打印结果集
        			sprintf(tem,"温度:%s,湿度:%s,PM2.5指数:%s", g_row[1], g_row[2],g_row[0]); // 第一，第二字段
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","CON_R");
				cJSON_AddStringToObject(json, "data",tem);
				out=cJSON_Print(json);
				send(clientList[index].socket, out, strlen(out), 0);
				printf("已发送给反馈程序%s:\n",out);
				memset(tem, 0, KEY_LENGTH);
				free(out);
				out=NULL;
			}else if(0==strcmp("G_DATA",item->valuestring))	//打开数据流
			{//  b8:27:eb:20:48:4b 52:54:00:cc:3b:e4
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","S_CON");
				cJSON_AddStringToObject(json, "data","G_DATA");
				out=cJSON_Print(json);
				encrypt_buf=getRightEncrypt(out,clientList[macInIndex].aes_key);
				send(clientList[macInIndex].socket, encrypt_buf, strlen(encrypt_buf), 0);
				//printf("....已发送%s\n",encrypt_buf);
				free(out);
				out=NULL;
				
			}else if(0==strcmp("S_DATA",item->valuestring))	//关闭数据流
			{//  b8:27:eb:20:48:4b 52:54:00:cc:3b:e4
				json=cJSON_CreateObject();	
				cJSON_AddStringToObject(json, "type","S_CON");
				cJSON_AddStringToObject(json, "data","S_DATA");
				out=cJSON_Print(json);
				encrypt_buf=getRightEncrypt(out,clientList[macInIndex].aes_key);
				send(clientList[macInIndex].socket, encrypt_buf, strlen(encrypt_buf), 0);
				///printf("....已发送%s\n",encrypt_buf);
				free(out);
				out=NULL;
				
			}else{
				printf("暂不处理的请求!\n");
			}
			
			
			break;
		default:
			printf("非法的请求!");
		}
		memset(recv_buf, 0, sizeof(recv_buf));
	}

	close(connfd); 		//关闭已连接套接字
	cJSON_Delete(json);	//清空json
	
	clientList[index].socket = -1;
	if(0!=strcmp(SIRI_NAME,clientList[index].nickName))
	{
		printf("%s[%s]下线!\n", clientList[index].nickName, clientList[index].ip);
		printf("当前在线数:%d\n", getClientCunt());
	}
	//memset(recv_buf, 0, sizeof(recv_buf));
	memset(clientList[index].nickName,0,USER_LENGTH_NAME);
	memset(clientList[index].aes_key,0,USER_LENGTH_KEY);
	memset(clientList[index].macAddr,0,USER_LENGTH_MAC);
	return NULL;
}

//获取请求类型
int getRequestType(char *str)
{
	if(0==strcmp(str,"PI_CONNECT")){		//树莓派连接请求
		return PI_CONNECT;
	}else if(0==strcmp(str,"SI_CONNECT")){	//Siri响应程序连接请求
		return SI_CONNECT;
	}else if(0==strcmp(str,"S_CON")){	//Siri控制请求
		return S_CON;
	}else if(0==strcmp(str,"DATA_ONLE")){	//Siri控制请求
		return DATA_ONLE;
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

int getIndexByMac(char *str)
{
	for (int i = 0; i < CLIENT_MAX_COUNT; i++)
	{
		if (clientList[i].socket>0&&0==strcmp(str,clientList[i].macAddr))
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
			printf("位置%d已占用,id:%d name:%s\n",i,clientList[i].socket,clientList[i].nickName);
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
	sprintf(hello, "%s[%s]上线!", name, ip);
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

void print_mysql_error(const char *msg)
 { // 打印最后一次错误
    if (msg)
        printf("%s: %s\n", msg, mysql_error(sqlCon));
    else
        puts(mysql_error(sqlCon));
}

int executesql(const char * sql) 
{
    /*query the database according the sql*/
    if (mysql_real_query(sqlCon, sql, strlen(sql))) // 如果失败
        return -1; // 表示失败

    return 0; // 成功执行
}


int init_mysql() 
{ // 初始化连接
    
    sqlCon = mysql_init(NULL);
    //设置字符编码,可能会乱码
   //mysql_query(sqlCon,"set nemas utf-8");
    
    if(!mysql_real_connect(sqlCon, g_host_name, g_user_name, g_password, g_db_name, g_db_port, NULL, 0)) // 如果失败
        return -1;
 
    // 是否连接已经可用
    if (executesql("set names utf8")) // 如果失败
       return -1;
    return 0; // 返回成功
}


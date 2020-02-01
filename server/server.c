#include <stdio.h>
#include <stdlib.h>
#include <string.h>						
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>


#define CLIENT_MAX_COUNT	5
#define MAXLINE			1024

//请求类型
#define	CONNECT 1	//请求连接
#define	NORMAL	2	//聊天内容
#define PUT_F	3	//请求上传文件
#define GET_F	4	//请求下载文件

typedef struct combine{
	int sockfd;
	int index;
	char filePath[128];
}scom;


typedef struct  _client_type
{
	char ip[INET_ADDRSTRLEN];	//字符串ip
	int socket;					//套接字
	char nickNme[30];			//昵称
	
}client;


void *threadConnect(void *vargp);
void *threadReceiveFile(void *vargp);
void sayHello(char *str,char *ip,int index);
void broadcast(char *str,int except);
int getClientCunt();
int getIndexBySocket(int socket);
int getFreeIndex();
int getRequestType(char *str);
void *threadSendFile(void * combine);
void *threadReceiveFile(void *combine);



client clientList[5]={{"",-1},{"",-1},{"",-1},{"",-1},{"",-1}};





int main(int argc, char *argv[])
{
	char recv_buf[2048] = "";			// 接收缓冲区
	int sockfd = 0;						// 套接字
	int connfd = 0;
	int err_log = 0;
	struct sockaddr_in my_addr;			// 服务器地址结构体
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
	
	//bzero(&my_addr, sizeof(my_addr));	     // 初始化服务器地址
	memset(&my_addr,0,sizeof(my_addr));

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
		struct sockaddr_in client_addr;		 		  // 用于保存客户端地址
		socklen_t cliaddr_len = sizeof(client_addr);      // 必须初始化!!!

		connfd = accept(sockfd, (struct sockaddr*)&client_addr, &cliaddr_len);       // 获得一个已经建立的连接
		if(connfd < 0)
		{
			perror("accept");
			continue;
		}

		int index=getFreeIndex();
		if(index==-1){
			printf("达到设定阈值!\n");
			break;
		}


		inet_ntop(AF_INET, &client_addr.sin_addr,clientList[index].ip , INET_ADDRSTRLEN);
		clientList[index].socket=connfd;		
		pthread_t tid;
		pthread_create(&tid, NULL, threadConnect, &connfd);
	}
	
	close(sockfd);         //关闭监听套接字
	return 0;
}


void *threadConnect(void *vargp){
	
	size_t recv_len = 0;
	char recv_buf[2048];			// 接收缓冲区
	char tem[2048];
	int connfd=*((int *)vargp);
	int index=getIndexBySocket(connfd);
	memset(recv_buf,0,sizeof(recv_buf));
		
	while((recv_len  = recv(connfd, recv_buf, sizeof(recv_buf), 0)) > 0)
	{

		int type=getRequestType(recv_buf);
		//printf("%s-+-%d\n", recv_buf,type);
		switch(type){
			case CONNECT:
				
				strcpy(clientList[index].nickNme,recv_buf+3);
				sayHello(recv_buf,clientList[index].ip,index);
				printf("新的连接:%ssocket\n",clientList[index].nickNme);//,clientList[index].socket
				printf("当前在线数:%d\n",getClientCunt());

				break;
			case NORMAL:
				memset(tem,0,sizeof(tem));
				sprintf(tem,"%s[%s]:%s",clientList[index].nickNme,clientList[index].ip,recv_buf+3);
				printf("转发消息 : %s\n",tem);
				broadcast(tem,index);
				break;
			case PUT_F:
				printf("文件上传请求!文件名:%s\n",recv_buf+3);
				scom sf;
				sf.index=index;
				sf.sockfd=clientList[index].socket;
				strcpy(sf.filePath,recv_buf+3);
				pthread_t t_receive_file;
				//printf("传给文件线程的到参数为:%s:%d\n",sf.filePath,sf.sockfd);
				pthread_create(&t_receive_file, NULL, threadReceiveFile, &sf);
				pthread_join(t_receive_file, NULL);
				break;
			case GET_F:
				printf("文件下载请求!文件名%s\n",recv_buf+3);
				char state[10];
				if(0!=access(recv_buf+3,R_OK)){
					strcpy(state,"_..err");
					send(clientList[index].socket, state, strlen(state), 0);
					
				}else{
					strcpy(state,"_..ok");
					send(clientList[index].socket, state, strlen(state), 0);
					//sleep(1);
					scom sf;
					sf.sockfd=clientList[index].socket;
					strcpy(sf.filePath,recv_buf+3);
					pthread_t t_send_file;
					//printf("传给文件线程的到参数为:%s:%d\n",sf.filePath,sf.sockfd);
					pthread_create(&t_send_file, NULL, threadSendFile, &sf);
					pthread_join(t_send_file, NULL);
				}
				
				break;		
			default:
			printf("非法的请求!");
		}
		memset(recv_buf,0,sizeof(recv_buf));
	}

	close(connfd);     //关闭已连接套接字
	clientList[index].socket=-1;
	printf("%s[%s]离开聊天室!\n",clientList[index].nickNme,clientList[index].ip);
	printf("当前在线数:%d\n",getClientCunt());
	memset(recv_buf,0,sizeof(recv_buf));
	memset(clientList[index].nickNme,0,sizeof(clientList[index].nickNme));
	

}

void *threadSendFile(void * combine)
{

	char tem[128];
	char send_buf[MAXLINE];
	char filePath[128];
	scom tc=*((scom *)combine);
	int sockfd = tc.sockfd;
	FILE *fp;

	sprintf(send_buf,".._%s",tc.filePath);

	if ((fp = fopen(tc.filePath,"r")) == NULL) 
	{  
    	perror("Open file failed\n");  
    	exit(0);  
	} 

	memset(send_buf,0,sizeof(send_buf));
	//printf("文件线程的到参数为:%s:%d\n",tc.filePath,sockfd);
	int send_len=0,read_len=0;
	while ((read_len = fread(send_buf, sizeof(char), MAXLINE, fp)) >0 ) 
	{  
		send_len = send(sockfd, send_buf, read_len, 0);  
		if ( send_len < 0 ) 
		{  
			perror("Send file failed\n");  
			exit(0);  
		}  

		memset(send_buf,0,sizeof(send_buf));
	} 
	printf("文件传输完毕!\n");

	sleep(1);
	send(sockfd, "---", 4, 0);
	fclose(fp);
}

void *threadReceiveFile(void *combine){

	char tem[128];
	char file_buf[MAXLINE];
	char filePath[128];
	scom tc=*((scom *)combine);
	int sockfd = tc.sockfd;
	FILE *fp;

	if ((fp = fopen(tc.filePath, "a+")) == NULL) 
	{  
   	 	perror("Open file failed\n");  
    	exit(0);  
	}

	memset(file_buf,0,sizeof(file_buf));
	int write_leng;
	while(recv(sockfd, file_buf, sizeof(file_buf), 0)){

		if(0==strcmp("---",file_buf))
			break;

		write_leng = fwrite(file_buf, sizeof(char), sizeof(file_buf), fp);  
		if (write_leng < sizeof(file_buf)) 
		{  
 		   printf("Write file failed\n");  
  		   break;  
		} 
		memset(file_buf,0,sizeof(file_buf));
	}


	fclose(fp);
	memset(tem,0,sizeof(tem));
	sprintf(tem,"%s上传了新文件:%s",clientList[tc.index].nickNme,tc.filePath);
	printf("文件保存成功!\n");
	broadcast(tem,tc.index);

}


int getRequestType(char *str){
	char tem[4];
	strncpy(tem,str,3);
	tem[3]='\0';
	//printf("得到了:%s\n",tem);
	if(0==strcmp(tem,"_._")){
		//连接请求
		return CONNECT;
	}else if(0==strcmp(tem,"__.")){
		//聊天消息
		return NORMAL;
	}else if(0==strcmp(tem,".._")){
		//请求上传文件
		return PUT_F;
	}else  if(0==strcmp(tem,"_..")){
		//请求下载文件
		return GET_F;
	}

	return 0;
}


int getFreeIndex()		//从列表中拿到一个可用的地址
{
	for(int i=0;i<CLIENT_MAX_COUNT;i++){
		//printf("%d\n",clientList[i].socket);
		if(clientList[i].socket==-1){
			return i;
		}
	}
	return -1;
}

int getIndexBySocket(int socket){
	for(int i=0;i<CLIENT_MAX_COUNT;i++){
		if(socket==clientList[i].socket){
			return i;
		}
	}
	return -1;
}

int getClientCunt()
{
	int n=0;
	for(int i=0;i<CLIENT_MAX_COUNT;i++){
		if(clientList[i].socket!=-1){
			n++;
		}
	}
	return  n;
}


//广播消息
void broadcast(char *str,int except){
	for(int i=0;i<CLIENT_MAX_COUNT;i++){
			if(i!=except && -1!=clientList[i].socket){
			send(clientList[i].socket, str, strlen(str), 0);
		}
	}
}


//连接时发送欢迎消息
void sayHello(char *str,char *ip,int index){
	char name[10];
	char hello[50];
	strcpy(name,str+3);
	sprintf(hello,"欢迎%s[%s]加入聊天室!",name,ip);
	broadcast(hello,index);
}

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


#define	MAX_MEM_COUNT	20	//内存中最大聊天记录个数
#define	MAX_LIST_COUNT	10	//客户端显示的最大聊天记录个数
#define MAXLINE			1024


int bolckCount=0;	//当前数据块个数
sem_t sem;			//信号量 终端使用权


typedef struct _chat_data{
	char time[10];
	char data[200];
	struct _chat_data *next;
	struct _chat_data *pre;
}chat;

typedef struct combine{
	int sockfd;
	char filePath[128];
}scom;

chat head;
void logChat(chat* head);
int sendfile(char *path,void *vargp);
void *threadSendFile(void *vargp);
void *threadReceiveFile(void *vargp);
void *threadsend(void *vargp);
void *threadrecv(void *vargp);
void getTime(char * psTime);
void getDate(char * psDate);
void addToList(char *data);
void uiCls();


int main(int argc, char *argv[])
{
	unsigned short port = 8000;        		// 服务器的端口号
	char *server_ip = "127.0.0.1";      // 服务器ip地址
	//char send_buf[512] = "";	
	//char recv_buf[512] = "";
	sem_init(&sem, 0, 1);
	char name[10];
	char temp[10];
	int sockfd = 0;
	int err_log = 0;
	struct sockaddr_in server_addr;
	head.next=NULL;
	head.pre=NULL;

	printf("请输入你的姓名:\n");
	scanf("%s",&name);

	memset(&server_addr,0,sizeof(server_addr));
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

	sprintf(temp,"_._%s",name);

	//向服务端发送个人信息
	send(sockfd, temp, strlen(temp), 0);   // 向服务器发送信息


	pthread_t tid1,tid2;
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

	printf("**********聊天室**********\n");
	chat *tem=&head;
	
	int count=bolckCount>MAX_LIST_COUNT-1?MAX_LIST_COUNT:bolckCount;
	for(int i=0;i<count&&tem;i++){
		tem=tem->next;
	}
	for(int i=0;i<count&&tem;i++){
		printf("%s\n",tem->data);
		tem=tem->pre;
	}
	while(count++<MAX_LIST_COUNT){
		printf("\n");
	}
	
	//printf("请发送:");
	sem_post(&sem);

}

void addToList(char *data)
{
	chat *tem;
	tem=(chat *)malloc(sizeof(chat));
	if(head.next)
			head.next->pre=tem;
	tem->next=head.next;
	head.next=tem;
	tem->pre=&head;
	getTime(tem->time);
	strcpy(tem->data,data);


	if(++bolckCount==MAX_MEM_COUNT)
	{
		printf("即将记录日志\n");
		logChat(&head);
		bolckCount=MAX_LIST_COUNT;
	}
}


void *threadsend(void * vargp)
{
	char tem[512];
	char send_buf[512];
	char filePath[128];
	int sockfd = *((int *)vargp);

	uiCls();
	getchar();
	while(1){

		gets(tem);

		if(strlen(tem)==0){
			uiCls();
			continue;
		}

		if(strlen(tem)==1&&tem[0]<33){
			if(tem[0]==6){		//Ctrl+f
				sem_wait(&sem);
				printf("请输入文件路径:");
				scanf("%s",&filePath);
				sem_post(&sem);
				sendfile(filePath,vargp);
			}else if(tem[0]==7)	{	//Ctrl+g
				char fn[50];
				sem_wait(&sem);
				printf("请输下载的文件名:");
				scanf("%s",&fn);
				sem_post(&sem);
				sprintf(tem,"_..%s",fn);
				
				send(sockfd, tem, strlen(tem), 0);//+1?
				}
			}else{
				sprintf(send_buf,"__.%s",tem);
				send(sockfd, send_buf, strlen(send_buf), 0);   // 向服务器发送信息
				sprintf(send_buf,"ME:%s",tem);
				addToList(send_buf);
				uiCls();
			}
		memset(send_buf,0,sizeof(send_buf));

	}
	
}


void *threadrecv(void *vargp)
{
	int sockfd = *((int *)vargp);
	char recv_buf[512];
	
	
	
	//bolckCount=0;
	while(1){
		recv(sockfd, recv_buf, sizeof(recv_buf), 0);   // 接收服务器发回的信息
		if(0==strcmp("_..ok",recv_buf)){
			pthread_t t_receive_file;
			printf("正在创建连接.....\n");
			scom cb;
			strcpy(cb.filePath,"...");
			cb.sockfd=sockfd;
			pthread_create(&t_receive_file, NULL, threadReceiveFile, &cb);
			pthread_join(t_receive_file, NULL);
		}else if(0==strcmp("_..err",recv_buf)){
			char tem[]="不存在的文件";
			addToList(tem);
			uiCls();
		}else{
			//printf("%s\n",recv_buf );
			addToList(recv_buf);
			uiCls();
		}
		

		memset(recv_buf,0,sizeof(recv_buf));
	}

	return NULL;
}

void getDate(char * psDate){
    time_t nSeconds;
    struct tm * pTM;
    
    time(&nSeconds); // 同 nSeconds = time(NULL);
    pTM = localtime(&nSeconds);
    
    /* 系统日期,格式:YYYMMDD */
    sprintf(psDate,"%04d-%02d-%02d", 
            pTM->tm_year + 1900, pTM->tm_mon + 1, pTM->tm_mday);
}
 
void getTime(char * psTime) {
    time_t nSeconds;
    struct tm * pTM;
    
    time(&nSeconds);
    pTM = localtime(&nSeconds);
    
    /* 系统时间，格式: HHMMSS */
    sprintf(psTime, "%02d:%02d:%02d",
            pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
}


void *threadReceiveFile(void *combine){
	//printf("00000!\n");

	char tem[128];
	char file_buf[MAXLINE];
	char filePath[128];


	scom tc=*((scom *)combine);
	int sockfd = tc.sockfd;
	FILE *fp;

	getTime(tc.filePath);
	sprintf(tem,"%s.txt",tc.filePath);

	strcpy(tc.filePath,tem);


	if ((fp = fopen(tc.filePath, "a+")) == NULL) 
	{  
   	 	perror("Open file failed\n");  
    	exit(0);  
	}

	memset(file_buf,0,sizeof(file_buf));
	int write_leng;
	while(recv(sockfd, file_buf, sizeof(file_buf), 0)){
		printf("收到%d字节流:%s\n", strlen(file_buf),file_buf);

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
	sprintf(tem,"文件下载完成,保存在当前路径下,文件名为:%s",tc.filePath);
	addToList(tem);
	uiCls();

}



int sendfile(char *path,void *vargp)
{
	printf("文件路径:%s\n", path);
	if(0!=access(path,R_OK)){
		printf("文件%s不存在!\n",path);
		return -1;
	}else{
		printf("文件%s存在!\n",path);
	}

	scom sf;
	sf.sockfd=*((int *)vargp);
	strcpy(sf.filePath,path);
	pthread_t t_send_file;
	pthread_create(&t_send_file, NULL, threadSendFile, &sf);
	pthread_join(t_send_file, NULL);
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
	send(sockfd, send_buf, strlen(send_buf), 0);   // 向服务器发送信息
	//sleep(1);

	if ((fp = fopen(tc.filePath,"r")) == NULL) 
	{  
    	perror("Open file failed\n");  
    	exit(0);  
	} 
	memset(send_buf,0,sizeof(send_buf));
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
	memset(tem,0,sizeof(tem));
	//printf("文件传输完毕!\n");
	sprintf(tem,"成功发送文件:%s",tc.filePath);
	addToList(tem);
	//uiCls();

	sleep(1);
	send(sockfd, "---", 4, 0);
	fclose(fp);
}


void logChat(chat* head)
{
	chat *finder=head;
	chat *temp,*tem;
	//找到开始持久化的节点
	for(int i=0;i<MAX_LIST_COUNT;i++){//&&finder
		finder=finder->next;
	}
	temp=finder->next;
	finder->next=NULL;
	temp->pre=NULL;

	//将指针定位到链表尾部
	while(temp->next){
		temp=temp->next;
	}


	FILE *fp;
	if ((fp = fopen("log.txt", "a+")) == NULL)
	{
		//printf("文件开始写入\n");
	}

	while(temp){
		tem=temp;
		if(tem->pre)
			tem->pre->next=NULL;
		temp=temp->pre;
		fprintf(fp,"%s - %s\n",tem->time,tem->data);
	 	free(tem);

	}
	
	fclose(fp);

}

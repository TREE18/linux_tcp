#include <stdio.h>
#include <sys/stat.h>
#include <arpa/inet.h>//htons
#include<netinet/in.h> // sockaddr_in 
#include<sys/types.h>  // socket 
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>//malloc
#include <string.h> //memset
#include <errno.h>
#include<sys/socket.h> // socket 
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define SERVER_PORT 5006 
#define BUFFER_SIZE 1024 
#define SERVER_INADDR "192.168.31.28"
#define PackHead_Size (sizeof(uint32_t))

struct Usr_Packet {
    uint32_t file_size;
    uint8_t  file_content[BUFFER_SIZE - PackHead_Size];
}__attribute__((packed));//__attribute__((packed)) 放在结构名 后面是要被忽略的// 注意：你需要设置结构字段对齐。

struct Usr_Val
{
	int ret;//返回值
	long cfd;
}Usr_val;
//**//全局变量的声明；
unsigned char buffer[BUFFER_SIZE];//注意0xff与char关系
struct sockaddr_in server_addr;
int sockfd = -1;
int des_fd;//生成目的文件的fd
uint32_t File_size = 0; 
uint32_t file_size_recv = 0;//接收到的文件大小
volatile char FileTransmitFlag = 0;
uint32_t Recv_Size = 0;
char * Usr_argv1 = NULL;
char * Usr_argv2 = NULL;

struct sockaddr_in clnaddr = {0};//存放客户的ip和端口
int clnaddr_size = sizeof(clnaddr);

struct Usr_Packet pth_UsrPacket;
struct Usr_Packet *Packet_p = NULL;
unsigned char *Heap_p = NULL; //申请堆的指针
//**//

void print_err(char *str, int line, int err_no)
{
        printf("%d, %s: %s\n", line, str, strerror(err_no));
        exit(-1);
}

void signal_fun(int signo)  
{
	if(signo == SIGINT)
	{	
		if(NULL != Heap_p)
		{
			free(Heap_p);
			Heap_p = NULL;
		}
		close(des_fd);
		shutdown(Usr_val.cfd, SHUT_RDWR);	
		exit(0);
	}
}

void Usr_Socket_Init(const char *SIP_p)
{
	/* 创建使用TCP协议通信的套接字文件 */
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) print_err("socket fail", __LINE__, errno);
	 
	bzero(&server_addr, sizeof(server_addr)); 
	server_addr.sin_family = AF_INET; 
	server_addr.sin_addr.s_addr = inet_addr(SIP_p); 
	server_addr.sin_port = htons(SERVER_PORT); 
}

void Usr_Socket_OtherAction()
{
	Usr_val.ret = bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(Usr_val.ret == -1) print_err("bind fail", __LINE__, errno);
	
	/* 讲主动的"套接字文件描述符"转为被动描述符,用于被动监听客户的连接 */
	Usr_val.ret = listen(sockfd, 3);
	if(Usr_val.ret== -1) print_err("listen fail", __LINE__, errno);
}

void *pth_handle(void *ptharg)
{
	long pth_handle_cfd = (long)ptharg;;
	printf("pth_handle_cfd is %ld\r\n",pth_handle_cfd);
	while(1)
	{
		Recv_Size = recv(pth_handle_cfd, buffer, 1024, 0);//recv都是有包头的

		if(Recv_Size <= 0  )
			{
				if(des_fd != 0)close(des_fd);
				shutdown(Usr_val.cfd, SHUT_RDWR);
				file_size_recv = File_size = FileTransmitFlag = 0;
				bzero(buffer, BUFFER_SIZE); 
	
				if(Recv_Size < 0)print_err("recv<0 transimit fail", __LINE__, errno);
				else	{break; printf("linkage interrupt\r\n");}//退出线程
				continue;
			}

		if(FileTransmitFlag == 0)
		{
			if(Recv_Size >= PackHead_Size)//说明有数据包来
			{
				printf("has recv : %d\r\n",Recv_Size);
				Packet_p = (struct Usr_Packet *)buffer;
				File_size = ntohl(Packet_p->file_size);
				printf("File_size of transimit is %d\r\n",File_size);
				FileTransmitFlag = 1;//上锁		这里的整个if只执行一次
				
				file_size_recv = Recv_Size - 4;//去包头

				des_fd = open(Usr_argv1, O_RDWR|O_CREAT|O_TRUNC, 0777); //ftruclearncate(des_fd,0); 
				if(des_fd < 0)print_err("des_fd fail", __LINE__, errno);
													
				Usr_val.ret = write(des_fd, Packet_p->file_content, file_size_recv);//write 无包头
				printf("has write : %d\r\n",Usr_val.ret);
				bzero(buffer, BUFFER_SIZE); 
			}	
		}
		else{	//上锁后执行这里
					Usr_val.ret = write(des_fd, Packet_p->file_content, Recv_Size - 4);//write Recv_Size要去包头
					printf("has write : %d\r\n",Usr_val.ret);
					file_size_recv = file_size_recv + (Recv_Size - 4);
		}	

		if(file_size_recv >= File_size)
			{
				close(des_fd);
				file_size_recv = File_size = FileTransmitFlag = 0;//reflash
				bzero(buffer, BUFFER_SIZE);
				if(file_size_recv == File_size)printf("transmit is success and close des_fd\r\n");		//transmit is ok
				else printf("transmit is fail and close des_fd\r\n");
			}
	}

	
	return NULL;
}

int main(int argc,char **argv)
{
	pthread_t tid;

	Usr_argv1 = argv[1];
		
	signal(SIGINT, signal_fun);

	printf("sizeof(uint32_t) is %ld\r\n",sizeof(uint32_t));
	printf("sizeof(struct Usr_Packet) is %ld \r\n",sizeof(struct Usr_Packet));
	
	Usr_Socket_Init(argv[2]);
	Usr_Socket_OtherAction();
	
	while(1)
	{
		Usr_val.cfd = accept(sockfd, (struct sockaddr *)&clnaddr, &clnaddr_size);
		if(Usr_val.cfd== -1) print_err("accept fail", __LINE__, errno);//打印客户的端口和ip, 一定要记得进行端序转换
		printf("sockfd is %d\n",sockfd);
		printf("clint_port = %d, clint_ip = %s\n", ntohs(clnaddr.sin_port), inet_ntoa(clnaddr.sin_addr));

		Usr_val.ret = pthread_create(&tid, NULL, pth_handle, (void *)(Usr_val.cfd));
		printf("tid is %ld\n",tid);
		if(Usr_val.ret != 0) print_err("pthread_create fail", __LINE__, Usr_val.ret);
	}
	return 0;
}








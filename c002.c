#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>//stat
#include <fcntl.h>//open 

#define SPORT 5006
#define BUFFER_SIZE 1024 
#define SIP "192.168.31.28"

int sockfd = -1;

 struct Usr_Packet {
    uint32_t file_size;
    uint8_t  file_content[BUFFER_SIZE - sizeof(uint32_t)];
}__attribute__((packed));//__attribute__((packed)) ���ڽṹ�� ������Ҫ�����Ե�// ע�⣺����Ҫ���ýṹ�ֶζ��롣

struct Usr_Packet *Packet_p = NULL;
unsigned char *Heap_p = NULL; //����ѵ�ָ��

void print_err(char *str, int line, int err_no)
{
        printf("%d, %s: %s\n", line, str, strerror(err_no));
        exit(-1);
}

void Usr_Heap_p_Free()
{
		if(NULL != Heap_p)
		{
			free(Heap_p);
			Heap_p= NULL;
		}
}


void signal_fun(int signo)
{
	if(SIGINT == signo)
	{
		/* �Ͽ����� */
		//close(sockfd);
		shutdown(sockfd,SHUT_RDWR);

		Usr_Heap_p_Free();

		if(NULL != Packet_p)
		{
			free(Packet_p);
			Packet_p= NULL;
		}
		exit(0);
	}
}



int main(int argc , char ** argv)
{
	uint32_t ret = 0;
	unsigned int debug_num = 0;
	char tmp_num;
	int test_fd;

	struct stat st;
	char transmitting_flag = 0;
	char htonl_first_flag = 0;
	uint32_t sum;

	printf("struct Usr_Packet size is %ld\r\n",sizeof(struct Usr_Packet));

	
	signal(SIGINT, signal_fun);

	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) print_err("socket fail", __LINE__, errno);
	
	/* ����connect,������������������� */
	struct sockaddr_in seraddr = {0};//���ڴ����Ҫ�������ӵ��Ǹ���������ip�Ͷ˿�
	
	seraddr.sin_family = AF_INET;//��ַ��
	seraddr.sin_port   = htons(SPORT);//����������Ķ˿�
	seraddr.sin_addr.s_addr = inet_addr(argv[2]);//��������ip,����ǿ���ͨ�ŵĻ�,���Ƿ������Ĺ���ip
	 
	ret = connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr));
	if(ret == -1) print_err("connect fail", __LINE__, errno);
	
#if 0
	pthread_t tid;
	ret = pthread_create(&tid, NULL, pth_fun, NULL);
	if(ret != 0) print_err("pthread_create fail", __LINE__, ret);
#endif

	Packet_p = malloc(sizeof(struct Usr_Packet));
	if(Packet_p == NULL)print_err("Packet_p malloc fail", __LINE__, errno);
	memset(Packet_p, 0, sizeof(struct Usr_Packet));

	while(1)
	{
		printf("input [y] with transmitting\n");	
		scanf("%c",&tmp_num);
		if(tmp_num == 'y'||tmp_num == 'Y')
			{
				transmitting_flag = 1;

				test_fd = open(argv[1], O_RDONLY);//"/home/gec/Desktop/xxx"
				if(test_fd < 0)print_err("open fail", __LINE__, errno);
				fstat(test_fd, &st);
				printf("%s size is %ld and test_fd is %d\r\n", argv[1], st.st_size, test_fd);

				Heap_p = malloc(st.st_size);
				if(Heap_p == NULL)print_err("Heap_p malloc fail", __LINE__, errno);
				memset(Heap_p, 0, st.st_size);
				
			}
		while(transmitting_flag)
			{
				if(st.st_size <= (BUFFER_SIZE - sizeof(uint32_t)))//�ļ�С��1020���ڴ˴���
					{
						printf("tranmitting: the file is smaller than 1020\r\n");
						ret = read(test_fd, Heap_p, st.st_size);//�ļ���С <= 1020 
						printf("has read : %d \r\n",ret);
						Packet_p->file_size = htonl(st.st_size);//��װ��
						memcpy( Packet_p->file_content, Heap_p, st.st_size);

						ret = send(sockfd, (void *)Packet_p, st.st_size + sizeof(uint32_t), 0);//st.st_size��Packet_p->file_contentͬ���� û��ͷ ��Ҫ����
						if(ret == -1) print_err("send fail", __LINE__, errno);
						else {
								printf("has send : %d \r\n",ret);
								transmitting_flag = tmp_num = 0; //��մ����� ��y����־
								memset(Heap_p, 0, st.st_size);	 
								close(test_fd);
								Usr_Heap_p_Free();
								
								if(ret == st.st_size + sizeof(uint32_t))printf("transimit is succcess and close test_fd\r\n");
								else printf("transimit is fail and close test_fd\r\n");
							}
					}
				else
					{//�ļ�����1020���ڴ˴���
							
							if(htonl_first_flag == 0)
								{
									printf("tranmitting: the file is bigger than 1020\r\n");
									Packet_p->file_size = htonl((uint32_t)st.st_size);//��һ�η�װ�ļ���Сͷ
									htonl_first_flag = 1;  //��
								}
							else Packet_p->file_size = htonl((uint32_t)0);//��װ��
	 
							if((st.st_size - sum) >= (BUFFER_SIZE - sizeof(uint32_t)))
							{
								read(test_fd, Heap_p, BUFFER_SIZE - sizeof(uint32_t));//��1020���ѻ���
								memcpy( Packet_p->file_content, Heap_p, BUFFER_SIZE - sizeof(uint32_t)); //��С��1021-1023��ע��
								ret = send(sockfd, (void *)Packet_p, BUFFER_SIZE, 0);	 //��������ȫ������
								
							}
							else {
								printf("the final transmit \r\n");
								read(test_fd, Heap_p, st.st_size - sum);//���ļ����ð�ͷ
								memcpy( Packet_p->file_content, Heap_p, (st.st_size - sum));//Packet_p->file_content =  Heap_p;
								ret = send(sockfd, (void *)Packet_p, (st.st_size - sum)+sizeof(uint32_t), 0);//���Ͳ�Ҫ���ϰ�ͷ
							} 	
							
							if(ret == -1) print_err("send fail", __LINE__, errno);
							else {
									usleep(1000);
									printf("ret is %d\r\n",ret);
						//			 printf("sum is %d\r\n",sum);
									 sum = sum + (ret - sizeof(uint32_t));//��ȥpack head
									 printf("sum is %d\r\n",sum);
									 
									if(sum == st.st_size)
										{
											transmitting_flag = tmp_num = sum = htonl_first_flag = 0;//reflash
											close(test_fd);
											Usr_Heap_p_Free();
											printf("transimit is succcess and close test_fd\r\n");
											break;
										}
									else if (sum > st.st_size)
										{
													printf("transimit is fail\r\n");
													transmitting_flag = tmp_num = sum = htonl_first_flag = 0;//reflash
													close(test_fd);
													Usr_Heap_p_Free();
													break;
									}
									
									memset(Heap_p, 0, st.st_size);		//reflash	
								}
					}
		}
	} 
	return 0;
}






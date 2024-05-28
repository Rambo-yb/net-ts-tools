#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "serial.h"

#define MAX_FD_NUM (10)
#define SERIAL_WRITE_BUFF_SIZE (1024*1024)
#define SERIAL_READ_BUFF_SIZE (16*1024)

typedef struct {
	int fd;
	int send_state;
	int recv_total_len;

	char ip[16];
	int port;
}SocketInfo;

typedef struct {
	int total_size;
	int cur_size;
	char* buff;
}SerialSpace;

typedef struct {
	int sock_fd;
	int serial_fd;
	pthread_mutex_t mutex;
	SerialSpace serial_write;
	SerialSpace serial_read;
}TransparentMng;
static TransparentMng kTransparentMng = {.mutex = PTHREAD_MUTEX_INITIALIZER};

int TcpServerCreate(int port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Failed to create socket");
        return -1;
    }
	
	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in)) < 0) {
        perror("Failed to bind address");
		close(sockfd);
        return -1;
    }
	
    if (listen(sockfd, MAX_FD_NUM) < 0) {
        perror("Failed to listen");
		close(sockfd);
        return -1;
    }
	
	return sockfd;
}

int TcpServerClose(int sockfd) {
	return close(sockfd);
}

void* ServerProc(void* arg) {
	int client_cnt = 0;
	SocketInfo cli_info[MAX_FD_NUM];
	fd_set read_fd;
	int max_fd = kTransparentMng.sock_fd < kTransparentMng.serial_fd ? kTransparentMng.serial_fd : kTransparentMng.sock_fd;
	int arr_fd[MAX_FD_NUM] = {0};
	int fd_send[MAX_FD_NUM] = {0};
	int total_size[MAX_FD_NUM] = {0};
	while(1) {
		FD_ZERO(&read_fd);
		FD_SET(kTransparentMng.sock_fd, &read_fd);
		FD_SET(kTransparentMng.serial_fd, &read_fd);
		
		for(int i = 0; i < MAX_FD_NUM; i++) {
			if (cli_info[i].fd > 0) {
				FD_SET(cli_info[i].fd, &read_fd);
				max_fd = max_fd < cli_info[i].fd ? cli_info[i].fd : max_fd;
			}
		}
		
		int ret = select(max_fd+1, &read_fd, NULL, NULL, NULL);
		if (ret < 0) {
			printf("select fail !\n");
			break;
		} else if (ret == 0) {
			// timeout
			continue;
		}
		
		if (FD_ISSET(kTransparentMng.sock_fd, &read_fd)) {
			struct sockaddr_in cli_addr;
			socklen_t cli_len = sizeof(struct sockaddr_in);
			int cli_fd = accept(kTransparentMng.sock_fd, (struct sockaddr *)&cli_addr, &cli_len);
			if (cli_fd < 0) {
				printf("server accept fail!\n");
				continue;
			}
			
			if (client_cnt >= MAX_FD_NUM) {
				printf("max client arrive!\n");
				close(cli_fd);
				continue;
			}
			
			for(int i = 0; i < MAX_FD_NUM; i++) {
				if (cli_info[i].fd == 0) {
					cli_info[i].fd = cli_fd;
					cli_info[i].send_state = 1;
					cli_info[i].port = ntohs(cli_addr.sin_port);
					snprintf(cli_info[i].ip, sizeof(cli_info[i].ip), "%s", inet_ntoa(cli_addr.sin_addr));
					break;
				}
			}
			
			client_cnt++;
			printf("we got a new connection, client_socket=%d, client_count=%d, ip=%s, port=%d\n", cli_fd, client_cnt, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		}
		
		if (FD_ISSET(kTransparentMng.serial_fd, &read_fd)) {
			char buff[1024] = {0};
			int read_len = read(kTransparentMng.serial_fd, buff, sizeof(buff));
			if (read_len <= 0) {
				
			} else {
				if (kTransparentMng.serial_read.cur_size + read_len <= kTransparentMng.serial_read.total_size) {
					memcpy(kTransparentMng.serial_read.buff+kTransparentMng.serial_read.cur_size, buff, read_len);
					kTransparentMng.serial_read.cur_size += read_len;
				} else {
					char* p = (char*) realloc(kTransparentMng.serial_read.buff, kTransparentMng.serial_read.total_size + read_len);
					if (p == NULL) {
						printf("space is not enough\n");
					} else {
						kTransparentMng.serial_read.buff = p;
						memset(kTransparentMng.serial_read.buff + kTransparentMng.serial_read.cur_size, 0, read_len);
						memcpy(kTransparentMng.serial_read.buff + kTransparentMng.serial_read.cur_size, buff, read_len);
						kTransparentMng.serial_read.cur_size += read_len;
						kTransparentMng.serial_read.total_size += read_len;
					}
				}
			}
		}
		
		for(int i = 0; i < MAX_FD_NUM; i++) {
			if (cli_info[i].fd == 0) {
				continue;
			}
			
			if (FD_ISSET(cli_info[i].fd, &read_fd)) {
				char buff[10240] = {0};
				int recv_len = recv(cli_info[i].fd, buff, sizeof(buff), 0);
				if (recv_len <= 0) {
					if (recv_len < 0) {
						perror("recv ");
					}

					printf("we delete connection, client_socket=%d, client_count=%d, ip=%s, port=%d, total_len:%d\n", cli_info[i].fd, client_cnt, cli_info[i].ip, cli_info[i].port, cli_info[i].recv_total_len);
					client_cnt--;
					close(cli_info[i].fd);
					FD_CLR(cli_info[i].fd, &read_fd);
					memset(&cli_info[i], 0, sizeof(SocketInfo));
				} else {
					cli_info[i].recv_total_len += recv_len;
					int write_len = write(kTransparentMng.serial_fd, buff, recv_len);

					// printf("write_len:%d len:%d total_size:%d\n", write_len, recv_len, total_size[i]);
        			// pthread_mutex_lock(&kTransparentMng.mutex);
					// if (kTransparentMng.serial_write.cur_size + recv_len <= kTransparentMng.serial_write.total_size) {
					// 	memcpy(kTransparentMng.serial_write.buff+kTransparentMng.serial_write.cur_size, buff, recv_len);
					// 	kTransparentMng.serial_write.cur_size += recv_len;
					// 	printf("recv_len:%d buff_size:%d total_size:%d\n", recv_len, kTransparentMng.serial_write.cur_size, total_size[i]);
					// } else {
    				// 	char* p = (char*) realloc(kTransparentMng.serial_write.buff, kTransparentMng.serial_write.total_size + recv_len);
					// 	if (p == NULL) {
					// 		printf("space is not enough\n");
					// 	} else {
					// 		kTransparentMng.serial_write.buff = p;
					// 		memset(kTransparentMng.serial_write.buff + kTransparentMng.serial_write.cur_size, 0, recv_len);
					// 		memcpy(kTransparentMng.serial_write.buff + kTransparentMng.serial_write.cur_size, buff, recv_len);
					// 		kTransparentMng.serial_write.cur_size += recv_len;
					// 		kTransparentMng.serial_write.total_size += recv_len;
					// 	}
					// }
        			// pthread_mutex_unlock(&kTransparentMng.mutex);
				}
			}
		}

		
		if (kTransparentMng.serial_read.cur_size != 0) {
			for(int i = 0; i < MAX_FD_NUM; i++) {
				if (cli_info[i].fd == 0 || !cli_info[i].send_state) {
					continue;
				}
				
				int send_len = send(cli_info[i].fd, kTransparentMng.serial_read.buff, kTransparentMng.serial_read.cur_size, 0);
				if (send_len < 0) {
					cli_info[i].send_state = 0;
				}
			}

			memset(kTransparentMng.serial_read.buff, 0, kTransparentMng.serial_read.cur_size);
			kTransparentMng.serial_read.cur_size = 0;
		}

		usleep(1*1000);
	}
	
}

// void* SerialProc(void* arg) {
// 	while (1) {
// 		usleep(500);
// 		if (kTransparentMng.serial_write.cur_size <= 0) {
// 			continue;
// 		}

// 		pthread_mutex_lock(&kTransparentMng.mutex);
// 		int len = kTransparentMng.serial_write.cur_size < 210 ? kTransparentMng.serial_write.cur_size : 210;
// 		int write_len = write(kTransparentMng.serial_fd, kTransparentMng.serial_write.buff, len);
// 		memcpy(kTransparentMng.serial_write.buff, kTransparentMng.serial_write.buff+write_len, kTransparentMng.serial_write.cur_size - write_len);
// 		kTransparentMng.serial_write.cur_size -= write_len;

// 		// printf("write_len:%d len:%d buff_size:%d\n", write_len, len, kTransparentMng.serial_write.cur_size);
// 		memset(kTransparentMng.serial_write.buff + kTransparentMng.serial_write.cur_size, 0, kTransparentMng.serial_write.total_size - kTransparentMng.serial_write.cur_size);
// 		pthread_mutex_unlock(&kTransparentMng.mutex);
// 	}
	
// }

int main(int argc, char** argv) {
	if (argc < 4) {
		printf("./serial_transparent <tcp_svr_port> <uart_path> <uart_baudrate>\n");
		printf("ex: ./serial_transparent 10000 /dev/ttyS3 115200\n");
		return -1;
	}
	
	char uart_path[128] = {0};
	int port = atoi(argv[1]);
    snprintf(uart_path, sizeof(uart_path), "%s", argv[2]);
	int baudrate = atoi(argv[3]);
	kTransparentMng.sock_fd = TcpServerCreate(port);
	kTransparentMng.serial_fd = SerialOpen(uart_path, baudrate);
	
	// kTransparentMng.serial_write.cur_size = 0;
	// kTransparentMng.serial_write.total_size = SERIAL_WRITE_BUFF_SIZE;
	// kTransparentMng.serial_write.buff = (char*)malloc(SERIAL_WRITE_BUFF_SIZE);
	// memset(kTransparentMng.serial_write.buff, 0, SERIAL_WRITE_BUFF_SIZE);

	kTransparentMng.serial_read.cur_size = 0;
	kTransparentMng.serial_read.total_size = SERIAL_READ_BUFF_SIZE;
	kTransparentMng.serial_read.buff = (char*)malloc(SERIAL_READ_BUFF_SIZE);
	memset(kTransparentMng.serial_read.buff, 0, SERIAL_READ_BUFF_SIZE);

	pthread_t pthread_id;
	pthread_create(&pthread_id, NULL, ServerProc, NULL);
	// pthread_t serial_pthread_id;
	// pthread_create(&serial_pthread_id, NULL, SerialProc, NULL);
	
	pthread_join(pthread_id, NULL);
	// pthread_join(serial_pthread_id, NULL);
	
	SerialClose(kTransparentMng.serial_fd);
	TcpServerClose(kTransparentMng.sock_fd);
	
	
	return 0;
}
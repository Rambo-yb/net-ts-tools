#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>

#include "serial.h"
#include "tcp_server.h"

#define SERIAL_WRITE_BUFF_SIZE (1024*1024)
#define SERIAL_READ_BUFF_SIZE (16*1024)
#define RECV_DATA_LEN (688130)

typedef struct {
	int fd;
	int send_state;

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
static TransparentMng kTransparentMng = {.mutex = PTHREAD_MUTEX_INITIALIZER, .serial_fd = -1};

long GetTime() {
    struct timeval time_;
    memset(&time_, 0, sizeof(struct timeval));

    gettimeofday(&time_, NULL);
    return time_.tv_sec*1000 + time_.tv_usec/1000;
}

void* ServerProc(void* arg) {
	int serial_clean_flag = 0;
	int client_cnt = 0;
	SocketInfo cli_info[MAX_CLIENT_NUM];
	fd_set read_fd;
	fd_set write_fd;
	int max_fd = kTransparentMng.sock_fd;
	int arr_fd[MAX_CLIENT_NUM] = {0};
	int fd_send[MAX_CLIENT_NUM] = {0};
	int total_size[MAX_CLIENT_NUM] = {0};
	while(1) {
		FD_ZERO(&read_fd);
		FD_SET(kTransparentMng.sock_fd, &read_fd);

		FD_ZERO(&write_fd);
		FD_SET(kTransparentMng.sock_fd, &write_fd);

		if (kTransparentMng.serial_fd != -1) {
			FD_SET(kTransparentMng.serial_fd, &read_fd);
			FD_SET(kTransparentMng.serial_fd, &write_fd);
			max_fd = max_fd < kTransparentMng.serial_fd ? kTransparentMng.serial_fd : max_fd;
		}
		
		for(int i = 0; i < MAX_CLIENT_NUM; i++) {
			if (cli_info[i].fd > 0) {
				FD_SET(cli_info[i].fd, &read_fd);
				FD_SET(cli_info[i].fd, &write_fd);
				max_fd = max_fd < cli_info[i].fd ? cli_info[i].fd : max_fd;
			}
		}
		
		int ret = select(max_fd+1, &read_fd, &write_fd, NULL, NULL);
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
			
			if (client_cnt >= MAX_CLIENT_NUM) {
				printf("max client arrive!\n");
				close(cli_fd);
				continue;
			}
			
			for(int i = 0; i < MAX_CLIENT_NUM; i++) {
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

			if (kTransparentMng.serial_fd == -1) {
				kTransparentMng.serial_fd = SerialOpen("/dev/ttyS3", 115200);
				printf("open serial ttys3\n");
			}
		}
		
		if (kTransparentMng.serial_fd != -1 && FD_ISSET(kTransparentMng.serial_fd, &read_fd)) {
			char buff[1024] = {0};
			int read_len = read(kTransparentMng.serial_fd, buff, sizeof(buff));
			if (read_len > 0) {
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
		
		for(int i = 0; i < MAX_CLIENT_NUM; i++) {
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

					printf("we delete connection, client_socket=%d, client_count=%d, ip=%s, port=%d\n", cli_info[i].fd, client_cnt, cli_info[i].ip, cli_info[i].port);
					client_cnt--;
					FD_CLR(cli_info[i].fd, &read_fd);
					close(cli_info[i].fd);
					memset(&cli_info[i], 0, sizeof(SocketInfo));
					if (client_cnt == 0) {
						SerialClose(kTransparentMng.serial_fd);
						kTransparentMng.serial_fd = -1;
					}
				} else {
					if (kTransparentMng.serial_fd == -1) {
						continue;
					}
					int write_len = write(kTransparentMng.serial_fd, buff, recv_len);
				}
			}
		}
		
		for(int i = 0; i < MAX_CLIENT_NUM; i++) {
			if (cli_info[i].fd == 0 || !cli_info[i].send_state) {
				continue;
			}
			
			if (FD_ISSET(cli_info[i].fd, &write_fd)) {
				if (kTransparentMng.serial_read.cur_size != 0) {
					serial_clean_flag = 1;
					int send_len = send(cli_info[i].fd, kTransparentMng.serial_read.buff, kTransparentMng.serial_read.cur_size, 0);
					if (send_len < 0) {
						cli_info[i].send_state = 0;
						continue;
					}
					printf("send serial data:%d\n", send_len);
				}
			}
		}

		if (serial_clean_flag) {
			serial_clean_flag = 0;
			memset(kTransparentMng.serial_read.buff, 0, kTransparentMng.serial_read.cur_size);
			kTransparentMng.serial_read.cur_size = 0;
		}

		usleep(1*1000);
	}
	
}

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
	// kTransparentMng.serial_fd = SerialOpen(uart_path, baudrate);

	kTransparentMng.serial_read.cur_size = 0;
	kTransparentMng.serial_read.total_size = SERIAL_READ_BUFF_SIZE;
	kTransparentMng.serial_read.buff = (char*)malloc(SERIAL_READ_BUFF_SIZE);
	memset(kTransparentMng.serial_read.buff, 0, SERIAL_READ_BUFF_SIZE);

	pthread_t pthread_id;
	pthread_create(&pthread_id, NULL, ServerProc, NULL);
	
	pthread_join(pthread_id, NULL);
	
	free(kTransparentMng.serial_read.buff);

	// SerialClose(kTransparentMng.serial_fd);
	TcpServerClose(kTransparentMng.sock_fd);
	
	
	return 0;
}
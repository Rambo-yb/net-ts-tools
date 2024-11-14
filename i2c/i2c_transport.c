#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>

#include "i2c.h"
#include "tcp_server.h"

typedef struct {
	int sock_fd;
    int i2c_fd;
    unsigned short i2c_addr;
	pthread_mutex_t mutex;
}TransportMng;
static TransportMng kTransportMng = {.mutex = PTHREAD_MUTEX_INITIALIZER};

typedef struct {
	int fd;
	int send_state;

	char ip[16];
	int port;
}SocketInfo;

static void* ServerProc(void* arg) {
	int client_cnt = 0;
	SocketInfo cli_info[MAX_CLIENT_NUM];
	fd_set read_fd;
	int max_fd = kTransportMng.sock_fd;
	while(1) {
		FD_ZERO(&read_fd);
		FD_SET(kTransportMng.sock_fd, &read_fd);
		
		for(int i = 0; i < MAX_CLIENT_NUM; i++) {
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
		    usleep(10*1000);
			continue;
		}
		
		if (FD_ISSET(kTransportMng.sock_fd, &read_fd)) {
			struct sockaddr_in cli_addr;
			socklen_t cli_len = sizeof(struct sockaddr_in);
			int cli_fd = accept(kTransportMng.sock_fd, (struct sockaddr *)&cli_addr, &cli_len);
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
		}

        for(int i = 0; i < MAX_CLIENT_NUM; i++) {
			if (cli_info[i].fd == 0) {
				continue;
			}
			
			if (FD_ISSET(cli_info[i].fd, &read_fd)) {
				char buff[1024] = {0};
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
				} else {
					if (strncmp(buff, "write_single:", strlen("write_single:")) == 0) {
						unsigned char val = 0;
						unsigned char reg = 0;
                        sscanf(buff, "%*[^:]: %02x %02x", &reg, &val);
						I2cWrite(kTransportMng.i2c_fd, kTransportMng.i2c_addr, reg, val);
					}else if (strncmp(buff, "write:", strlen("write:")) == 0) {
						int offset = strlen("write:");
						char i2c_buff[1024] = {0};
						i2c_buff[0] = 0x47;
						memcpy(i2c_buff+1, buff+offset, recv_len-offset);
						i2c_buff[recv_len-offset+1] = 0x5a;
                        I2cWriteArr(kTransportMng.i2c_fd, kTransportMng.i2c_addr, 0x00, i2c_buff, recv_len-offset+2);
                    } else if(strncmp(buff, "read:", strlen("read:")) == 0) {
                    	unsigned char reg = 0;
						int size = 1;
                        sscanf(buff, "%*[^:]: %02x %d", &reg, &size);
						memset(buff, 0, sizeof(buff));
                        I2cReadArr(kTransportMng.i2c_fd, kTransportMng.i2c_addr, reg, buff, size);
                        send(cli_info[i].fd, buff, size, 0);
                    }
				}
			}
		}

		usleep(10*1000);
	}
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("./i2c_transport <tcp_svr_port> <i2c-X> <i2c_addr>\n");
		printf("ex: ./i2c_transport 10000 i2c-0 0x67\n");
		return -1;
    }


	printf("%s %s %s\n", argv[1], argv[2], argv[3]);

    int port = atoi(argv[1]);
    sscanf(argv[3], "0x%x", &kTransportMng.i2c_addr);

    kTransportMng.sock_fd = TcpServerCreate(port);

	printf("argv[2]:%s\n", argv[2]);
    kTransportMng.i2c_fd = I2cOpen(argv[2]);
	if (kTransportMng.i2c_fd <= 0) {
		perror("");
	}

    pthread_t pthread_id;
	pthread_create(&pthread_id, NULL, ServerProc, NULL);
	
	pthread_join(pthread_id, NULL);

	I2cClose(kTransportMng.i2c_fd);
    
	TcpServerClose(kTransportMng.sock_fd);

    return 0;
}
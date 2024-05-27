#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

#define MAX_FD_NUM (10)

int sock_fd = 0;
int serial_fd = 0;

int baud_rate_consts[] = {B1500000, B115200, B9600};
int baud_rate_values[] = {1500000, 115200, 9600};

typedef struct  {
	int iBaudRate;
	int iDataBit;
	int iStopBit;
	int iParity;
	int iFlowCtrl;
}SERIAL_CONF;

static int SerialSetConf(int iFd, SERIAL_CONF stSerialConf) {
	int i;
	int status;
	struct termios opt;
	char parity;
	
	memset(&opt , 0, sizeof(struct termios));
	tcgetattr(iFd, &opt);
	for(i = 0; i < (sizeof(baud_rate_values) / sizeof(int)); i++) {
		if(stSerialConf.iBaudRate == baud_rate_values[i]) {
			tcflush(iFd, TCIOFLUSH);
			cfsetispeed(&opt, baud_rate_consts[i]);
			cfsetospeed(&opt, baud_rate_consts[i]);
			status = tcsetattr(iFd, TCSANOW, &opt);
			if (status != 0) {
				printf("tcsetattr abnormal!\n");
				return -1;
			}
			tcflush(iFd, TCIOFLUSH);
			break;
		}
	}
	
	switch (stSerialConf.iParity) {
	case 0:
		parity = 'N';
		break;
	case 1:
		parity = 'O';
		break;
	case 2:
		parity = 'E';
		break;
	default:
		printf("parity is not supported\n");
		break;
	}
	
	memset(&opt, 0, sizeof(opt));
	if (tcgetattr(iFd, &opt) != 0) {
		printf("tcgetattr abnormal!\n");
		return -1;
	}
	
	opt.c_cflag &= ~CSIZE;
	switch (stSerialConf.iDataBit) {
	case 5:
		opt.c_cflag |= CS5;
		break;
	case 6:
		opt.c_cflag |= CS6;
		break;
	case 7:
		opt.c_cflag |= CS7;
		break;
	case 8:
		opt.c_cflag |= CS8;
		break;
	default:
		printf("Unsupported data bits\n");
		return -1;
	}
	
	switch (parity) {
	case 'N':
		opt.c_cflag &= ~PARENB; /* Clear parity enable */
		opt.c_iflag &= ~INPCK; /* Enable parity checking */
		break;
	case 'O':
		opt.c_cflag |= (PARODD | PARENB); 
		opt.c_iflag |= INPCK; /* Disnable parity checking */
		break;
	case 'E':
		opt.c_cflag |= PARENB; /* Enable parity */
		opt.c_cflag &= ~PARODD;
		opt.c_iflag |= INPCK; /* Disnable parity checking */
		break;
	default:
		printf("Unsupported parity\n");
		return -1;
	}

	switch (stSerialConf.iStopBit) {
	case 1:
		opt.c_cflag &= ~CSTOPB;
		break;
	case 2:
		opt.c_cflag |= CSTOPB;
		break;
	default:
		printf("Unsupported stop bits\n");
		return -1;
	}
	
	if (parity != 'N') {
		opt.c_iflag |= INPCK;
	}
	opt.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
	opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	opt.c_oflag &= ~OPOST;

	tcflush(iFd, TCIFLUSH);
	opt.c_cc[VTIME] = 0;
	opt.c_cc[VMIN] = 0;

	if (tcsetattr(iFd, TCSANOW, &opt) != 0) {
		printf("Serial config failed!\n");
		return -1;
	}
	return 0;
}

int SerialOpen(char *pFilePath, int iBaudRate)
{
	int fd = open(pFilePath, O_RDWR);
	if(fd < 0) {
		return -1;
	}
	
	SERIAL_CONF stSerialConf = {iBaudRate, 8, 1, 0, 0};
	int ret = SerialSetConf(fd, stSerialConf);
	if(ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

void SerialClose(int iFd) {
	close(iFd);
}

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
	fd_set tmp_fd;
	int max_fd = sock_fd < serial_fd ? serial_fd : sock_fd;
	int arr_fd[MAX_FD_NUM] = {0};
	
	while(1) {
		FD_ZERO(&tmp_fd);
		FD_SET(sock_fd, &tmp_fd);
		FD_SET(serial_fd, &tmp_fd);
		
		for(int i = 0; i < MAX_FD_NUM; i++) {
			if (arr_fd[i] > 0) {
				FD_SET(arr_fd[i], &tmp_fd);
				max_fd = max_fd < arr_fd[i] ? arr_fd[i] : max_fd;
			}
		}
		
		int ret = select(max_fd+1, &tmp_fd, NULL, NULL, NULL);
		if (ret < 0) {
			printf("select fail !\n");
			break;
		} else if (ret == 0) {
			// timeout
			continue;
		}
		
		if (FD_ISSET(sock_fd, &tmp_fd)) {
			struct sockaddr_in cli_addr;
			socklen_t cli_len = sizeof(struct sockaddr_in);
			int cli_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &cli_len);
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
				if (arr_fd[i] == 0) {
					arr_fd[i] = cli_fd;
					break;
				}
			}
			
			client_cnt++;
			printf("we got a new connection, client_socket=%d, client_count=%d, ip=%s, port=%d\n", cli_fd, client_cnt, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		}
		
		if (FD_ISSET(serial_fd, &tmp_fd)) {
			char buff[1024] = {0};
			int read_len = read(serial_fd, buff, sizeof(buff));
			if (read_len <= 0) {
				
			} else {
				for(int i = 0; i < MAX_FD_NUM; i++) {
					if (arr_fd[i] == 0) {
						continue;
					}
					
					int send_len = send(arr_fd[i], buff, read_len, 0);
					if (send_len <= 0) {
						
					} else {
						// printf("send_len:%d read_len:%d send:%s\n", send_len, read_len, buff);
						
					}
				}
			}
		}
		
		for(int i = 0; i < MAX_FD_NUM; i++) {
			if (arr_fd[i] == 0) {
				continue;
			}
			
			if (FD_ISSET(arr_fd[i], &tmp_fd)) {
				char buff[1024] = {0};
				int recv_len = recv(arr_fd[i], buff, sizeof(buff) - 1, 0);
				if (recv_len < 0) {
					
				} else if (recv_len == 0) {
					client_cnt--;
					close(arr_fd[i]);
					FD_CLR(arr_fd[i], &tmp_fd);
					arr_fd[i] = 0;
				} else {
					int write_len = write(serial_fd, buff, recv_len);
					// printf("recv_len:%d write_len:%d recv:%s\n", recv_len, write_len, buff);
				}
			}
		}
		usleep(20*1000);
	}
	
}

int main(int argc, char** argv) {
	/*
	if (argc < 4) {
		printf("./test_serial_to_network <tcp_svr_port> <uart_path> <uart_baudrate>\n");
		printf("ex: ./test_serial_to_network 10000 /dev/ttyS3 115200\n");
		return -1;
	}
	
	char uart_path[128] = {0};
	int port = atoi(argv[1]);
    snprintf(uart_path, sizeof(uart_path), "%s", argv[2]);
	int baudrate = atoi(argv[3]);
	sock_fd = TcpServerCreate(port);
	serial_fd = SerialOpen(uart_path, baudrate);
	
	pthread_t pthread_id;
	pthread_create(&pthread_id, NULL, ServerProc, NULL);
	
	pthread_join(pthread_id, NULL);
	
	SerialClose(serial_fd);
	TcpServerClose(sock_fd);
	*/
	
	serial_fd = SerialOpen("/dev/ttyS3", 115200);
	char buff[8] = {0xaa, 0xc4, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc4};
	// char buff[8] = {0xaa, 0xc4, 0x01, 0x00, 0x00, 0x00, 0x00, 0xc5};
	int ret = write(serial_fd, buff, sizeof(buff));
	printf("ret:%d\n", ret);
	
	
	return 0;
}
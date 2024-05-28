#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "serial.h"

typedef struct  {
	int baud_rate;
	int data_bit;
	int stop_bit;
	int parity;
	int flow_ctrl;
}SerialConf;

static int kBaudRateConsts[] = {B1500000, B115200, B9600};
static int kBaudRateValues[] = {1500000, 115200, 9600};

static int SerialSetConf(int fd, SerialConf serial_conf) {
	int i;
	int status;
	struct termios opt;
	char parity;
	
	memset(&opt , 0, sizeof(struct termios));
	tcgetattr(fd, &opt);
	for(i = 0; i < (sizeof(kBaudRateValues) / sizeof(int)); i++) {
		if(serial_conf.baud_rate == kBaudRateValues[i]) {
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&opt, kBaudRateConsts[i]);
			cfsetospeed(&opt, kBaudRateConsts[i]);
			status = tcsetattr(fd, TCSANOW, &opt);
			if (status != 0) {
				printf("tcsetattr abnormal!\n");
				return -1;
			}
			tcflush(fd, TCIOFLUSH);
			break;
		}
	}
	
	switch (serial_conf.parity) {
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
	if (tcgetattr(fd, &opt) != 0) {
		printf("tcgetattr abnormal!\n");
		return -1;
	}
	
	opt.c_cflag &= ~CSIZE;
	switch (serial_conf.data_bit) {
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

	switch (serial_conf.stop_bit) {
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

	tcflush(fd, TCIFLUSH);
	opt.c_cc[VTIME] = 0;
	opt.c_cc[VMIN] = 0;

	if (tcsetattr(fd, TCSANOW, &opt) != 0) {
		printf("Serial config failed!\n");
		return -1;
	}
	return 0;
}

int SerialOpen(char *path, int baud_rate)
{
	int fd = open(path, O_RDWR);
	if(fd < 0) {
		return -1;
	}
	
	SerialConf serial_conf = {baud_rate, 8, 1, 0, 0};
	int ret = SerialSetConf(fd, serial_conf);
	if(ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

void SerialClose(int fd) {
	close(fd);
}
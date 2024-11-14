#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "i2c.h"

int I2cOpen(char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        return -1;
    }

    ioctl(fd, I2C_TIMEOUT, 2);

    return fd;
}

void I2cClose(int fd) {
    close(fd);
}

int I2cRead(int fd, unsigned short addr, unsigned char reg, unsigned char *buf) {
#if 0
    struct i2c_msg msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = malloc(1);
    if(msgs[0].buf == NULL) {
        return -1;
    }
    msgs[0].buf[0] = reg;

    msgs[1].addr = addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = malloc(1);
    if(msgs[1].buf == NULL) {
        free(msgs[0].buf);
        return -1;
    }
    msgs[1].buf[0] = 0x0;

    struct i2c_rdwr_ioctl_data data;
    memset(&data, 0, sizeof(data));
    data.nmsgs = 2;
    data.msgs = msgs;
    int ret = ioctl(fd, I2C_RDWR, &data);
    if (ret < 0)
        perror("I2cRead");

    *buf = msgs[1].buf[0];

    free(msgs[0].buf);
    free(msgs[1].buf);
    return ret;
#endif 
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        perror("set slave");
        return -1;
    }

    if (write(fd, &reg, 1) < 0) {
        perror("write reg addr fail");
        return -1;
    }

    if (read(fd, buf, 1) < 0) {
        perror("read reg data fail");
        return -1;
    }

    return 0;
}

int I2cReadArr(int fd, unsigned short addr, unsigned char reg, unsigned char* buf, int size) {
    for(int i = 0; i < size && i < 0xff; i++) {
        if (I2cRead(fd, addr, reg+i, &buf[i]) < 0) {
            break;
        }
    }
    return 0;
}

int I2cWrite(int fd, unsigned short addr, unsigned char reg, unsigned char buf) {
    struct i2c_msg msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = 2;
    msgs[0].buf = malloc(2);
    if (msgs[0].buf == NULL) {
        return 0;
    }
    msgs[0].buf[0] = reg;
    msgs[0].buf[1] = buf;

    struct i2c_rdwr_ioctl_data data;
    memset(&data, 0, sizeof(data));
    data.nmsgs = 1;
    data.msgs = msgs;
    int ret = ioctl(fd, I2C_RDWR, &data);
    if (ret < 0)
        perror("I2cWrite");

    free(msgs[0].buf);
    return ret;
}

int I2cWriteArr(int fd, unsigned short addr, unsigned char reg, unsigned char* buf, int size) {
    printf("write arr[%d]:", size);
    for(int i = 0; i < size && i < 0xff; i++) {
        if (I2cWrite(fd, addr, reg+i, buf[i]) < 0) {
            break;
        }
        printf("%02x ", buf[i]);
    }
    printf("\n");
    return 0;
}
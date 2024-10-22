#include <stdio.h>
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
    return open(path, O_RDWR);
}

void I2cClose(int fd) {
    close(fd);
}

int I2cRead(int fd, unsigned short addr, unsigned char reg, char *buf, int size) {
    struct i2c_msg msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    msgs[1].addr = addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = size;
    msgs[1].buf = buf;

    struct i2c_rdwr_ioctl_data data;
    memset(&data, 0, sizeof(data));
    data.nmsgs = 2;
    data.msgs = msgs;
    int ret = ioctl(fd, I2C_RDWR, &data);

    return ret;
}

int I2cWrite(int fd, unsigned short addr, unsigned char reg, char *buf, int size) {
    struct i2c_msg msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = size;
    msgs[0].buf = buf;

    struct i2c_rdwr_ioctl_data data;
    memset(&data, 0, sizeof(data));
    data.nmsgs = 1;
    data.msgs = msgs;
    int ret = ioctl(fd, I2C_RDWR, &data);

    return ret;
}
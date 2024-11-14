#ifndef __I2C_H__
#define __I2C_H__

int I2cOpen(char *path);

void I2cClose(int fd);

int I2cRead(int fd, unsigned short addr, unsigned char reg, unsigned char *buf);

int I2cReadArr(int fd, unsigned short addr, unsigned char reg, unsigned char* buf, int size);

int I2cWrite(int fd, unsigned short addr, unsigned char reg, unsigned char buf);

int I2cWriteArr(int fd, unsigned short addr, unsigned char reg, unsigned char* buf, int size);

#endif
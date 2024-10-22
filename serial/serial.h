#ifndef __SERIAL_H__
#define __SERIAL_H__

int SerialOpen(char *path, int baud_rate);

void SerialClose(int fd);

#endif
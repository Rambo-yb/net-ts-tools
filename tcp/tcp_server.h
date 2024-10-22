#ifndef __TCP_H__
#define __TCP_H__

#define MAX_CLIENT_NUM (10)

int TcpServerCreate(int port);

int TcpServerClose(int sockfd);

#endif
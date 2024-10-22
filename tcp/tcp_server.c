#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>

#include "tcp_server.h"

int TcpServerCreate(int port) {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("Failed to create socket");
        return -1;
    }
	
	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));

	struct sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr_in)) < 0) {
        perror("Failed to bind address");
		close(sockfd);
        return -1;
    }
	
    if (listen(sockfd, MAX_CLIENT_NUM) < 0) {
        perror("Failed to listen");
		close(sockfd);
        return -1;
    }
	
	return sockfd;
}

int TcpServerClose(int sockfd) {
	return close(sockfd);
}

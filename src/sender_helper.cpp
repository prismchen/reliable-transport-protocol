#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "data_struct.h"
#include "sender_helper.h"

extern int sockfd;
extern struct addrinfo hints, *servinfo, *p;
extern int rv;
extern FILE *fd;
extern char *filename;

extern unsigned long ack_num;

unsigned long *ack_buf = (unsigned long*) malloc(sizeof(unsigned long));
struct sockaddr remote_addr;
socklen_t remote_addr_len = sizeof remote_addr;

unsigned long get_file_size(const char* filename) {
	unsigned long size;
	FILE *f;

	f = fopen(filename, "rb");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}

int send_buf(char *buf) {

	int numbytes;
	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0, p->ai_addr, p->ai_addrlen)) == -1) {
		perror("sender: sendto");
		exit(1);
	}
	return numbytes;
} 

int recv_to_buf(char *buf) {
	int numbytes;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN , 0, &remote_addr, &remote_addr_len)) == -1) {
		// nothing
	}
	else {
		buf[numbytes] = '\0';
	}
	return numbytes;
}


void send_packet(packet* pck) {
	if (sendto(sockfd, pck, sizeof *pck, 0, p->ai_addr, p->ai_addrlen) == -1) {
		perror("sender: sendto");
		exit(1);
	}
}

unsigned long recv_ack() {
	int recv_len;
	recv_len = recvfrom(sockfd, ack_buf, sizeof ack_buf, 0, &remote_addr, &remote_addr_len);
	if (recv_len == -1) {
		perror("recvfrom timeout");
		exit(1);
	}
	if (recv_len != sizeof(unsigned long)) {
		perror("recv_ack");
		exit(1);
	}
	return *ack_buf;
}

int connect_prepare(char *ip) {

	fd = fopen(filename, "r");

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(ip, SERVERPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("sender: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "sender: failed to create socket\n");
		return 2;
	}

	return 0;
}

void clean_up() {
	freeaddrinfo(servinfo);
	close(sockfd);
	fclose(fd);
}
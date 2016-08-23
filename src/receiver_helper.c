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
#include "receiver_helper.h"

extern int sockfd;
extern struct addrinfo hints, *servinfo, *p;
extern int rv;
extern struct sockaddr_storage their_addr;
extern socklen_t addr_len;
extern FILE *fd;

void *get_in_addr(struct sockaddr *sa) { // get sockaddr, IPv4 or IPv6:
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int buf_recv(char *buf) {
	int numbytes;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN , 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}
	buf[numbytes] = '\0';
	return numbytes;
}

packet *buf_recv_packet() {
	packet* pck = (packet*) malloc(MAX_UDP + HEADER_SIZE);
	memset(pck, '\0', MAX_UDP + HEADER_SIZE);

	if (recvfrom(sockfd, pck, MAX_UDP + HEADER_SIZE , 0,
		(struct sockaddr *)&their_addr, &addr_len) == -1) {
		perror("recvfrom");
		exit(1);
	}
	return pck;
}

int prepare() {

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("receiver: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("receiver: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "receiver: failed to bind socket\n");
		return 2;
	}

	addr_len = sizeof their_addr;

	printf("receiver: waiting to recvfrom...\n");

	return 0;
}

void clean_up() {
	freeaddrinfo(servinfo);
	close(sockfd);
	fclose(fd);
}
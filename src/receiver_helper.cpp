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

extern unsigned long expected_sequence_num;

void *get_in_addr(struct sockaddr *sa) { // get sockaddr, IPv4 or IPv6:
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int send_buf(char *buf) {

	int numbytes;
	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &their_addr, addr_len)) == -1) {
		perror("sender: sendto");
		exit(1);
	}
	return numbytes;
} 

int recv_to_buf(char *buf) {
	int numbytes;
	if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN , 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recv_to_buf");
		exit(1);
	}
	buf[numbytes] = '\0';
	return numbytes;
}

packet *recv_packet() {
	packet* pck = (packet*) malloc(MAX_UDP + HEADER_SIZE);
	memset(pck, '\0', MAX_UDP + HEADER_SIZE);

	if (recvfrom(sockfd, pck, MAX_UDP + HEADER_SIZE , 0, (struct sockaddr *)&their_addr, &addr_len) == -1) {
		perror("recv_packet");
		exit(1);
	}
	return pck;
}

void send_ack() {
	if (sendto(sockfd, &expected_sequence_num, sizeof expected_sequence_num, 0, (struct sockaddr *) &their_addr, addr_len) == -1) {
		perror("send_ack");
		exit(1);
	}
	printf("send ack %lu\n", expected_sequence_num);
}

void write_to_file(packet *pck) {
	fwrite(pck->data, 1, pck->packet_size, fd);
	if (ferror(fd)) {
		perror("write_to_file");
		exit(4);
	}
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

	addr_len = sizeof their_addr; // You must also initialize fromlen to be the size of from or struct sockaddr

	printf("receiver: waiting to recvfrom...\n");

	return 0;
}

void clean_up() {
	freeaddrinfo(servinfo);
	close(sockfd);
	fclose(fd);
}
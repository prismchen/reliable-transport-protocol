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

#define MYPORT "4950"	// the port users will be connecting to
#define MAXBUFLEN 256

void *get_in_addr(struct sockaddr *sa);
int buf_recv(char *buf); 


int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
struct sockaddr_storage their_addr;
socklen_t addr_len;


int main(void) {

	int numbytes;
	int numbytes_total;
	int numbytes_recved = 0;
	char buf[MAXBUFLEN];
	FILE *fd;

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

	freeaddrinfo(servinfo);

	printf("receiver: waiting to recvfrom...\n");

	addr_len = sizeof their_addr;

	// receive file name
	buf_recv(buf);
	fd = fopen(buf, "w");

	// receive number of bytes
	buf_recv(buf);
	numbytes_total = atoi(buf);

	for (;;) {
		if (numbytes_recved >= numbytes_total) {
			break;
		}
		numbytes = buf_recv(buf);
		numbytes_recved += numbytes;
		
		// printf("%s", buf);
		fprintf(fd, "%s", buf);
		// printf("Received %d bytes\n", numbytes);
	}

	printf("Received %d bytes in total\n", numbytes_recved);

	close(sockfd);
	fclose(fd);
	return 0;
}

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
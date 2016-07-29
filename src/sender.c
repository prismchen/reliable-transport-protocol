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

#define SERVERPORT "4950"	// the port users will be connecting to

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	int numbytes_total = 0;

	if (argc != 3) {
		fprintf(stderr,"usage: sender hostname filename\n");
		exit(1);
	}

	// open file
	FILE *fd;
	fd = fopen(argv[2], "r");

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0) {
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

	// send file to receiver
	char buf[256];
	int is_filename_sent = 0;
	while (fgets(buf, 256, fd) != NULL) 
	{
		if (!is_filename_sent) 
		{
			if ((numbytes = sendto(sockfd, argv[2], strlen(argv[2]), 0, 
				p->ai_addr, p->ai_addrlen)) == -1) 
			{
				perror("sender: sendto");
				exit(1);
			}
			is_filename_sent = 1;
		}
		else 
		{
			if ((numbytes = sendto(sockfd, buf, strlen(buf), 0, 
				p->ai_addr, p->ai_addrlen)) == -1) 
			{
				perror("sender: sendto");
				exit(1);
			}
		}
		numbytes_total += numbytes;
	}

	freeaddrinfo(servinfo);

	printf("sender: sent %d bytes to %s\n", numbytes_total, argv[1]);
	close(sockfd);

	fclose(fd);
	return 0;
}
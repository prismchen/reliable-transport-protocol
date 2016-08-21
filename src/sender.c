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
#define MAXBUFLEN 256

int get_file_size(const char* filename);
int buf_send(char *buf);

int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;

int main(int argc, char *argv[]) {

	if (argc != 3) {
		fprintf(stderr,"usage: sender hostname filename\n");
		exit(1);
	}

	int numbytes; // number of bytes sent each UDP packet
	int numbytes_total = get_file_size(argv[2]); // number of bytes in file
	char buf[MAXBUFLEN];
	int numbytes_sent = 0; // number of bytes actually sent 
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

	// Send file name to receiver 

	buf_send(argv[2]);

	// Send number of bytes to receiver
	snprintf(buf, MAXBUFLEN, "%d", numbytes_total);
	buf_send(buf);

	
	while (fgets(buf, MAXBUFLEN, fd) != NULL) {
		numbytes = buf_send(buf);
		// printf("%s", buf);
		// printf("sent %d bytes\n", numbytes);
		numbytes_sent += numbytes;
	}

	freeaddrinfo(servinfo);

	printf("sender: sent %d bytes to %s\n", numbytes_sent, argv[1]);
	
	close(sockfd);
	fclose(fd);
	return 0;
}

int get_file_size(const char* filename) {
	int size;
	FILE *f;

	f = fopen(filename, "rb");
	if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);

    return size;
}

int buf_send(char *buf) {
	int numbytes;
	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0, 
		p->ai_addr, p->ai_addrlen)) == -1) {
		perror("sender: sendto");
		exit(1);
	}
	return numbytes;
} 
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
#include <pthread.h>

#include "data_struct.h"
#include "receiver_helper.h"

#define ARRAY_SIZE(x) sizeof x / sizeof ((*x))

// extern-ed global variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
struct sockaddr_storage their_addr;
socklen_t addr_len;
FILE *fd;

int file_size_in_bytes;

void *recv_file_thread(void *param) {

	int numbytes_recved = 0;
	char buf[MAXBUFLEN];

	int prepare_return;
	if ((prepare_return = prepare()) != 0) {
		perror("prepare failed");
		exit(prepare_return);
	}

	// receive file name
	buf_recv(buf);
	fd = fopen(buf, "w");

	// receive number of bytes
	buf_recv(buf);
	file_size_in_bytes = atoi(buf);
	printf("Number of bytes expected to receive %d\n", file_size_in_bytes);

	packet *pck;
	for (;;) {
		if (numbytes_recved >= file_size_in_bytes) {
			break;
		}
		pck = buf_recv_packet();

		numbytes_recved += pck->packet_size;
		
		// printf("%s", buf);
		fwrite(pck->data, 1, pck->packet_size, fd);
		if (ferror(fd)) {
			perror("Write error");
			exit(4);
		}
		// printf("Received %d bytes\n", numbytes);
	}

	printf("Received %d bytes in total\n", numbytes_recved);

	clean_up();

	pthread_exit(0);
}

int main(void) {

	pthread_t recv_tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_create(&recv_tid, &attr, recv_file_thread, NULL);

	pthread_join(recv_tid, NULL);

	return 0;
}


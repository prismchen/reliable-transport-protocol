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
#include "sender_helper.h"


// extern-ed global variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
FILE *fd; // file descriptor of the transfering file
char *filename;

int file_size_in_bytes;

void *send_file_thread(void *param) {

	char *ip = (char*) param;
	char buf[MAXBUFLEN];
	int numbytes_sent = 0; // number of bytes actually sent 

	int prepare_return;
	if ((prepare_return = connect_prepare(ip)) != 0) {
		perror("connect_prepare failed");
		exit(prepare_return);
	}

	// Send file name to receiver 

	buf_send(filename);

	// Send number of bytes to receiver
	snprintf(buf, MAXBUFLEN, "%d", file_size_in_bytes);
	buf_send(buf);

	packet* pck = (packet*) malloc(MAX_UDP + HEADER_SIZE);

	for (;;) {

		memset(pck, '\0', MAX_UDP + HEADER_SIZE);

		if (feof(fd)) {
			break;
		}

		int numbytes_read = fread(pck->data, sizeof(char), MAX_UDP, fd);

		if (ferror(fd)) {
			perror("Read error");
			exit(3);
		}

		pck->sequence_num = 0;
		pck->packet_size = numbytes_read;
		pck->file_size = file_size_in_bytes;

		buf_send_packet(pck);

		// printf("%s", buf);
		// printf("sent %d bytes\n", numbytes);
		numbytes_sent += numbytes_read;
	}

	printf("sender: sent %d bytes to %s\n", numbytes_sent, ip);

	clean_up();

	pthread_exit(0);
}

int main(int argc, char *argv[]) { // main function

	if (argc != 3) {
		fprintf(stderr,"usage: sender hostname filename\n");
		exit(1);
	}

	filename = argv[2];
	file_size_in_bytes = get_file_size(argv[2]); // number of bytes in file

	pthread_t send_tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_create(&send_tid, &attr, send_file_thread, argv[1]);

	pthread_join(send_tid, NULL);
	
	return 0;
}

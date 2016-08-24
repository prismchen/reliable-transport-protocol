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
#include <map>

#include "data_struct.h"
#include "receiver_helper.h"

using namespace std;

#define ARRAY_SIZE(x) sizeof x / sizeof ((*x))

// extern-ed global variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
struct sockaddr_storage their_addr;
socklen_t addr_len;
FILE *fd;

int file_size_in_bytes = 1; // cannot use 0 or not initializing

volatile unsigned long last_writable_sequence_num = 0;
unsigned long first_writable_sequence_num = 0; 
unsigned long expected_sequence_num = 0;
std::map<unsigned long, packet*> write_buffer;
volatile int recv_start = 0;

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
		if (expected_sequence_num == file_size_in_bytes) {
			break;
		}
		pck = buf_recv_packet();
		if (pck->sequence_num == expected_sequence_num) {
			write_buffer.insert(std::pair<unsigned long, packet*> (pck->sequence_num, pck));
			last_writable_sequence_num = expected_sequence_num;

			expected_sequence_num += pck->packet_size;

			send_ack();

			if (!recv_start) {
				recv_start = 1;
			}
		}
		
		numbytes_recved += pck->packet_size;
		
	}

	printf("Received %d bytes in total\n", numbytes_recved);

	printf("Reciver recv_thread finished...\n");
	pthread_exit(0);
}

void *write_file_thread(void *param) {
	std::map<unsigned long, packet*> :: iterator it;
	for (;;) {
		if (recv_start) {
			while (first_writable_sequence_num <= last_writable_sequence_num) {
				it = write_buffer.find(first_writable_sequence_num);
				if (it != write_buffer.end()) {
					write_to_file(it->second);
					first_writable_sequence_num += it->second->packet_size;
				}
				else {
					perror("packet lost");
					exit(5);
				}
			}
		}
		// sleep(1);
		// printf("first_writable_sequence_num is %lu\n", first_writable_sequence_num);
		if (first_writable_sequence_num == file_size_in_bytes) {
			break;
		}
	} 
	
	printf("Reciver write_thread finished...\n");
	pthread_exit(0);
}

int main(void) {

	pthread_t recv_tid;
	pthread_t write_tid;

	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_create(&recv_tid, &attr, recv_file_thread, NULL);
	pthread_create(&write_tid, &attr, write_file_thread, NULL);

	pthread_join(recv_tid, NULL);
	pthread_join(write_tid, NULL);

	clean_up();

	return 0;
}


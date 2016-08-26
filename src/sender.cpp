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
#include <time.h>
#include <map>

#include "data_struct.h"
#include "sender_helper.h"

using namespace std;

// extern-ed global variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
FILE *fd; // file descriptor of the transfering file
char *filename;

unsigned long window = 10*MAX_UDP;

int file_size_in_bytes;
// unsigned long global_sequence_num = 0;
unsigned long first_sequence_num_notsent = 0;
unsigned long last_sequence_num_notsent = 0; 
unsigned long largest_sequence_num_allowedtosend = window;
unsigned long ack_num = 0;
std::map<unsigned long, packet*> read_buffer;

volatile double sample_rtt = 0.1;
volatile clock_t timing_start, timing_end;
volatile clock_t timeout_start, timeout_end;
volatile int send_start = 0;
volatile int is_timing = 0;
volatile int is_timeout_running = 0;
volatile int timeout = 0;
volatile unsigned long timed_ack_num;

double alpha = 0.25;


void check_timeout() {
	timeout_end = clock();
	if ((double) (timeout_end - timeout_start)/CLOCKS_PER_SEC > 2.0 * sample_rtt) {
		timeout = 1;
	}
	else {
		timeout = 0;
	};
}

void reset_timeout() {
	timeout_start = clock();
}

void *send_file_thread(void *param) {

	char buf[MAXBUFLEN];

	// Send file name to receiver 

	buf_send(filename);

	// Send number of bytes to receiver
	snprintf(buf, MAXBUFLEN, "%d", file_size_in_bytes);
	buf_send(buf);

	packet* pck;
	std::map<unsigned long, packet*> :: iterator it;

	for (;;) {
		if(send_start) {
			if (!timeout) {
				while (first_sequence_num_notsent <= largest_sequence_num_allowedtosend && first_sequence_num_notsent <= last_sequence_num_notsent) {
					it = read_buffer.find(first_sequence_num_notsent);
					if (it != read_buffer.end()) {

						pck = it->second;
						buf_send_packet(pck);

						if (!is_timeout_running) {
							reset_timeout();
							is_timeout_running = 1;
						} 
						else {
							check_timeout();
						}

						if (!is_timing) {
							timed_ack_num = pck->sequence_num + pck->packet_size;
							timing_start = clock();
							is_timing = 1;
						}

						first_sequence_num_notsent += pck->packet_size;
					}
					else {
						perror("packet not found");
						exit(1);
					}
					
				}
			}
			else { // timeout! 
				printf("Retransmitting packet %lu\n", ack_num);
				it = read_buffer.find(ack_num);
				if (it != read_buffer.end()) {
					pck = it->second;
					buf_send_packet(pck);
					reset_timeout();
				}
				else {
					perror("packet not found");
					exit(1);
				}
				check_timeout();
			}
		}

		if (first_sequence_num_notsent == file_size_in_bytes) {
			break;
		}
	}

	printf("Sender: sent %lu bytes in total\n", first_sequence_num_notsent);
	printf("Sender send_file_thread finished...\n");
	pthread_exit(0);
}

void *recv_ack_thread(void *param) {

	unsigned long old_ack_num = ack_num;

	for (;;) {
		if (ack_num == file_size_in_bytes) {
			break;
		}
		ack_num = recv_ack();
		if (is_timing && timed_ack_num == ack_num) {
			timing_end = clock();
			sample_rtt = sample_rtt*(1 - alpha) + alpha* (double) (timing_end - timing_start)/CLOCKS_PER_SEC;
			// printf("sample_rtt is: %f\n", sample_rtt);
			is_timing = 0;
		}
		if (ack_num > old_ack_num) {
			reset_timeout();
			old_ack_num = ack_num;
		} 
		largest_sequence_num_allowedtosend = ack_num + window;
		// printf("receive ack num: %lu\n", ack_num);
	}

	printf("Sender recv_ack_thread finished...\n");
	pthread_exit(0);
}

void *read_file_thread(void *param) {

	unsigned long next_byte_toread = 0; 
	int numbytes_read;

	packet* pck;

	for (;;) {

		if (next_byte_toread == file_size_in_bytes) {
			break;
		}

		pck = (packet*) malloc(MAX_UDP + HEADER_SIZE);
		memset(pck, '\0', MAX_UDP + HEADER_SIZE);

		if (feof(fd)) {
			break;
		}

		numbytes_read = fread(pck->data, sizeof(char), MAX_UDP, fd);
		if (ferror(fd)) {
			perror("Read error");
			exit(3);
		}

		pck->sequence_num = next_byte_toread;
		pck->packet_size = numbytes_read;
		pck->file_size = file_size_in_bytes;

		read_buffer.insert(std::pair<unsigned long, packet*> (next_byte_toread, pck));

		if (!send_start) {
			send_start = 1;
		}

		last_sequence_num_notsent = next_byte_toread;

		next_byte_toread += numbytes_read;
	}

	

	printf("Sender read_file_thread finished...\n");
	pthread_exit(0);
}

int main(int argc, char *argv[]) { // main function

	if (argc != 3) {
		fprintf(stderr,"usage: sender hostname filename\n");
		exit(1);
	}

	filename = argv[2];
	file_size_in_bytes = get_file_size(argv[2]); // number of bytes in file


	int prepare_return;
	if ((prepare_return = connect_prepare(argv[1])) != 0) {
		perror("connect_prepare failed");
		exit(prepare_return);
	}

	pthread_t read_tid;
	pthread_t send_tid;
	pthread_t recv_tid;

	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_create(&read_tid, &attr, read_file_thread, NULL);
	pthread_create(&send_tid, &attr, send_file_thread, NULL);
	pthread_create(&recv_tid, &attr, recv_ack_thread, NULL);

	pthread_join(read_tid, NULL);
	pthread_join(send_tid, NULL);
	pthread_join(recv_tid, NULL);

	clean_up();
	
	return 0;
}

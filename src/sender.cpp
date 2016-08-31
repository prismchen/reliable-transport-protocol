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
#include <cmath>

#include "data_struct.h"
#include "sender_helper.h"

using namespace std;

// extern-ed global variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
FILE *fd; // file descriptor of the transfering file
char *filename;

unsigned long cwnd = 1*MAX_UDP;
unsigned long ssthresh = 64000;
int dup_ack_count = 0;

unsigned long file_size_in_bytes;
unsigned long first_sequence_num_notsent = 0;
unsigned long last_sequence_num_notsent = 0; 
unsigned long largest_sequence_num_allowedtosend = cwnd;
unsigned long ack_num = 0;
std::map<unsigned long, packet*> read_buffer;


volatile double estimated_rtt = 0.1;
volatile double dev_rtt = 0.0;
volatile double timeout_interval = estimated_rtt;

volatile clock_t timing_start;
volatile clock_t timeout_start;
volatile unsigned long timed_ack_num;

volatile int sender_state = SLOW_START;
volatile int send_start = 0;
volatile int recv_start = 0;
volatile int is_timing = 0;
volatile int is_timeout_running = 0;
volatile int timeout = 0;

double alpha = 0.5;
double beta = 0.25; 

void set_cwnd(unsigned long size) {
    cwnd = size;
    largest_sequence_num_allowedtosend = ack_num + cwnd;
}

void check_timeout() {
    if (((double) clock() - timeout_start)/CLOCKS_PER_SEC > timeout_interval) {
	dup_ack_count = 0;
	ssthresh = cwnd/2;
	set_cwnd(MAX_UDP);
	sender_state = SLOW_START;
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

    struct timeval sock_timeout;      
    sock_timeout.tv_sec = 1;
    sock_timeout.tv_usec = 0;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&sock_timeout, sizeof sock_timeout) < 0) {
        perror("setsockopt failed\n");
    }

	char buf[MAXBUFLEN];
	memset(buf, '\0', MAXBUFLEN);

	// Send file name to receiver 
	for (;;) {
		send_buf(filename);
		printf("Waiting for ack of filename...\n");
		if (recv_to_buf(buf) == -1) {
			continue;
		} 
		if (strcmp(buf, filename) == 0) {
			break;
		}
	}

	packet* pck = NULL;
	std::map<unsigned long, packet*> :: iterator it;

	while (!send_start) {}

	recv_start = 1;

	for (;;) {
		if (!timeout) {
			if (sender_state == FAST_RECOVERY) { // fast recovery/fast retransmission.
				is_timing = 0;

				it = read_buffer.find(ack_num);
				if (it != read_buffer.end()) {
					pck = it->second;

					send_packet(pck);

					usleep(0.5*estimated_rtt);

					// printf("Fast retransmitting packet %lu\n", ack_num);
				}
			}
			else {
				while (first_sequence_num_notsent <= largest_sequence_num_allowedtosend && first_sequence_num_notsent <= last_sequence_num_notsent) {
					it = read_buffer.find(first_sequence_num_notsent);
					if (it != read_buffer.end()) {

						if (!is_timing && pck) {
							timed_ack_num = pck->sequence_num + pck->packet_size;
							timing_start = clock();
							is_timing = 1;
						}

						pck = it->second;
						send_packet(pck);

						// printf("Send packet with sequence_num %lu\n", pck->sequence_num);

						if (!is_timeout_running) {
							reset_timeout();
							is_timeout_running = 1;
						} 

						first_sequence_num_notsent += pck->packet_size;
					}
				}
			}
		}
		else { // timeout
			is_timing = 0;

			it = read_buffer.find(ack_num);
			if (it != read_buffer.end()) {
				pck = it->second;
				send_packet(pck);

				// printf("Retransmitting packet %lu\n", ack_num);

				reset_timeout();
			}
			else {
				continue;
			}
		}
		check_timeout();

		if (ack_num == file_size_in_bytes) {
			break;
		}
	}

	printf("Sender: sent %lu bytes in total\n", first_sequence_num_notsent);
	printf("Sender: send_file_thread finished...\n");
	pthread_exit(0);
}

void *recv_ack_thread(void *param) {

	unsigned long old_ack_num = ack_num;
	double sample_rtt;

	while (!recv_start) {}

	for (;;) {
		if (ack_num == file_size_in_bytes) {
			break;
		}
		ack_num = recv_ack();

		if (is_timing && timed_ack_num <= ack_num) {
			sample_rtt = ((double) clock() - timing_start)/CLOCKS_PER_SEC;
			estimated_rtt = (1.0 - alpha)*estimated_rtt + alpha*sample_rtt;
			dev_rtt = (1.0 - beta)*dev_rtt + beta*(std::abs(estimated_rtt - sample_rtt));
			timeout_interval = estimated_rtt + 4 * dev_rtt;
			is_timing = 0;
			// printf("estimated_rtt is: %f ms\n", estimated_rtt * 1000);
		}

		if (ack_num > old_ack_num) { // new ack
			old_ack_num = ack_num;
			reset_timeout();

			if (sender_state == SLOW_START) {
				set_cwnd(cwnd + MAX_UDP);
				dup_ack_count = 0;
				if (cwnd >= ssthresh) {
					sender_state = CONGESTION_AVOIDANCE;
				}
			}
			else if (sender_state == CONGESTION_AVOIDANCE) {
				if (cwnd != 0) {
					set_cwnd(cwnd + MAX_UDP*MAX_UDP/cwnd);
				}
				else {
					set_cwnd(MAX_UDP);
				}
				dup_ack_count = 0;
			}
			else if (sender_state == FAST_RECOVERY) {
				set_cwnd(ssthresh);
				dup_ack_count = 0;
				sender_state = CONGESTION_AVOIDANCE;
			}
			else {
				perror("Invalid sender state");
				exit(1);
			}
		} 
		else if (ack_num == old_ack_num) { // duplicate ack
			dup_ack_count++;
			if (dup_ack_count == 3 && sender_state != FAST_RECOVERY) {
				sender_state = FAST_RECOVERY;
				ssthresh = cwnd/2;
				set_cwnd(ssthresh + 3*MAX_UDP);
				continue;
			}
			if (sender_state == FAST_RECOVERY) {
				set_cwnd(cwnd + MAX_UDP);
			}
		}
		else { // ack_num < old_ack_num
			// ignore
		}
	}

	printf("Sender: recv_ack_thread finished...\n");
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

	clock_t program_start;
	double program_duration;

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

	program_start = clock();

	pthread_create(&read_tid, &attr, read_file_thread, NULL);
	pthread_create(&send_tid, &attr, send_file_thread, NULL);
	pthread_create(&recv_tid, &attr, recv_ack_thread, NULL);

	pthread_join(read_tid, NULL);
	pthread_join(send_tid, NULL);
	pthread_join(recv_tid, NULL);

	clean_up();

	program_duration = ((double) clock() - program_start)/CLOCKS_PER_SEC;
	printf("Throughput: %.02f kB/s and duration: %.02f s\n", file_size_in_bytes/1000/program_duration, program_duration);
	
	return 0;
}

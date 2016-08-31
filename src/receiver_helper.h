#ifndef __RECEIVER_HELPER__
#define __RECEIVER_HELPER__

#define MYPORT "4950"	// the port users will be connecting to
#define MAXBUFLEN 256

void *get_in_addr(struct sockaddr *sa);
int send_buf(char *buf);
int recv_to_buf(char *buf); 
packet *recv_packet();
void send_ack();
void write_to_file(packet *pck);
int prepare();
void clean_up();
void *recv_file_thread(void *param);

#endif

#ifndef __RECEIVER_HELPER__
#define __RECEIVER_HELPER__

#define MYPORT "4950"	// the port users will be connecting to
#define MAXBUFLEN 256

void *get_in_addr(struct sockaddr *sa);
int buf_recv(char *buf); 
packet *buf_recv_packet();
int prepare();
void clean_up();
void *recv_file_thread(void *param);

#endif

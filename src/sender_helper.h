#ifndef __SENDER_HELPER__
#define __SENDER_HELPER__

#define SERVERPORT "4950"	// the port users will be connecting to
#define MAXBUFLEN 256
#define SLOW_START 1
#define CONGESTION_AVOIDANCE 2
#define FAST_RECOVERY 3


unsigned long get_file_size(const char* filename);
int buf_send(char *buf);
void buf_send_packet(packet* pck);
unsigned long recv_ack();
int connect_prepare(char *ip);
void clean_up(); // clean up all allocated resources
void *send_file_thread(void *param);
#endif
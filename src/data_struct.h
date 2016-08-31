#ifndef __DATA_STRUCT__
#define __DATA_STRUCT__

#define MAX_UDP 1448
#define HEADER_SIZE 24 // 3 x unsigned long

typedef struct packet
{
	unsigned long sequence_num;
	unsigned long packet_size;
	unsigned long file_size;
	char data[MAX_UDP];
} packet;

#endif
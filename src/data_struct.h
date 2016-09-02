/**
	@author Xiao Chen
*/
#ifndef __DATA_STRUCT__
#define __DATA_STRUCT__

#define MAX_UDP 1448 // size of data carried in each packet
#define HEADER_SIZE 24 // size of packet header: 3 x unsigned long

typedef struct packet
{
	unsigned long sequence_num;
	unsigned long packet_size;
	unsigned long file_size;
	char data[MAX_UDP];
} packet;

#endif
#ifndef __MSG_H_
#define __MSG_H_


#define DIR_U2S	0	// user -> server
#define DIR_S2U	1	// server -> user
#define DIR_S2R	2	// server -> router
#define DIR_R2S	3	// router -> server

#define TYPE_CONNECT	0
#define TYPE_DATA		1
#define TYPE_HEART		2

struct msg {
	unsigned char direction;
	unsigned char type;
	unsigned int data_len;
	char pack[2];
	char buf[0];
};


#endif

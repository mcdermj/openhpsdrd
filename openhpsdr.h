#ifndef _OPENHPSDR_H
#define _OPENHPSDR_H 1

typedef struct _metisDataHeader {
    u_int16_t magic;
    u_int8_t opcode;
    u_int8_t endpoint;
    u_int32_t sequence; 
} __attribute__((packed)) MetisDataHeader;

typedef struct _metisDiscoveryRequest {
    u_int16_t magic;
    u_int8_t opcode;
    u_int8_t padding[60];
} __attribute__((packed)) MetisDiscoveryRequest;

typedef struct _metisDiscoveryReply {
    u_int16_t magic;
    u_int8_t status;
    u_int8_t mac[6];
    u_int8_t version;
    u_int8_t boardid;
    u_int8_t padding[49];
} __attribute__((packed)) MetisDiscoveryReply;

typedef struct _metisStartStop {
    u_int16_t magic;
    u_int8_t opcode;
    u_int8_t startStop;
    u_int8_t padding[60];
} __attribute__((packed)) MetisStartStop;

typedef struct _ozySamplesIn {
	u_int8_t i[3];
	u_int8_t q[3];
	u_int16_t mic;
} __attribute__((packed)) OzySamplesIn;

typedef struct _ozySamplesOut {
	int16_t leftRx;
	int16_t rightRx;
	int16_t leftTx;
	int16_t rightTx;
} __attribute__((packed)) OzySamplesOut;

typedef struct _ozyPacket {
	u_int8_t magic[3];
	u_int8_t header[5];
	union _samples {
		OzySamplesIn in[63];
		OzySamplesOut out[63];
	} samples;
} __attribute__((packed)) OzyPacket;

typedef struct _metisPacket {
    u_int16_t magic;
    u_int8_t opcode;
    OzyPacket packets[2];
} __attribute__((packed)) MetisPacket;

#endif /* openhpsdr.h */

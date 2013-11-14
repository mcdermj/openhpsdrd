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

typedef struct _metisPacket {
    u_int16_t magic;
    u_int8_t opcode;
    u_int8_t padding[1029];
} __attribute__((packed)) MetisPacket;

#endif /* openhpsdr.h */

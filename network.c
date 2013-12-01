/*
 * network.c
 *
 *  Created on: Nov 30, 2013
 *      Author: mcdermj
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "openhpsdr.h"
#include "network.h"

#define SYNC 0x7F

static struct sockaddr_in *clientAddr = NULL;
static int IQThreadRunning = 0;
static pthread_t IQTransmitThread;
static int serviceSocket;

void socketServiceLoop(short port) {
    ssize_t bytesReceived;
    struct sockaddr_in bindAddress;
    struct sockaddr_in packetAddress;
    socklen_t packetAddressLength;
    MetisPacket receivedPacket;
    const int one = 1;

    if((serviceSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Service Socket: ");
        exit(1);
    }

    bindAddress.sin_family = AF_INET;
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddress.sin_port = htons(port);

    //  Set some socket options
    if(setsockopt(serviceSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        perror("Setting REUSEADDR: ");
        exit(1);
    }

    if(bind(serviceSocket, (struct sockaddr *) &bindAddress, sizeof(bindAddress)) == -1) {
        perror("Bind Socket: ");
        exit(1);
    }

    for(;;) {
        bytesReceived = recvfrom(serviceSocket, &receivedPacket, sizeof(receivedPacket), 0, (struct sockaddr *) &packetAddress, &packetAddressLength);
        if(bytesReceived == -1) {
            //  Need to trap EAGAIN or EWOULDBLOCK
            perror("Receiving packet:");
            exit(1);
        }

        switch(receivedPacket.header.opcode) {
        	case 0x01:
        		// printf("Data packet received\n");
        		break;
            case 0x02:
                discoveryHandler((MetisDiscoveryRequest *) &receivedPacket, &packetAddress, serviceSocket);
                break;
            case 0x04:
                startStopHandler((MetisStartStop *) &receivedPacket, &packetAddress, serviceSocket);
                break;
            default:
                printf("Unknown packet received\n");
                break;
        }
    }

    close(serviceSocket);
}


void discoveryHandler(MetisDiscoveryRequest *request, struct sockaddr_in *clientAddr, int serviceSocket) {
    char ipString[16];
    MetisDiscoveryReply replyPacket;
    struct ifreq buffer;
    // int i;
    ssize_t bytesWritten;

    if(inet_ntop(AF_INET, &clientAddr->sin_addr.s_addr, ipString, sizeof(ipString)) == NULL) {
        strcpy(ipString, "0.0.0.0\0");
    }

    printf("Discovery packet received from %s\n", ipString);

    replyPacket.magic = htons(0xEFFE);

    //  Whether the device is sending
    //  0x03 for yes, and 0x02 for no.
    replyPacket.status = 0x02;

    //  Find the MAC Address of eth0 on the box
    //  We probably shouldn't assume that we're on eth0 but oh well
    memset(&buffer, 0x00, sizeof(buffer));
    strcpy(buffer.ifr_name, "eth0");
    ioctl(serviceSocket, SIOCGIFHWADDR, &buffer);
    memcpy(replyPacket.mac, buffer.ifr_hwaddr.sa_data, 6);

    //  Version should be derived from somewhere sane.
    replyPacket.version = 0x1A;

    replyPacket.boardid = 0x05;

    memset(&replyPacket.padding, 0x00, sizeof(replyPacket.padding));

    bytesWritten = sendto(serviceSocket, &replyPacket, sizeof(replyPacket), 0, (struct sockaddr *) clientAddr, sizeof(struct sockaddr_in));
    if(bytesWritten == -1) {
        perror("Discovery Reply: ");
        return;
    }

    printf("Sent reply of %d bytes to %s on port %d\n", (int) bytesWritten, ipString, clientAddr->sin_port);
}

void startStopHandler(MetisStartStop *request, struct sockaddr_in *addr, int serviceSocket) {
    printf("Entering Start/Stop Handler\n");

    if(request->startStop && 0x01 == 0x01) {
    	if(clientAddr != NULL) {
    		fprintf(stderr, "Receiver already in use\n");
    		return;
    	}
    	clientAddr = malloc(sizeof(struct sockaddr_in));
    	memcpy(clientAddr, addr, sizeof(struct sockaddr_in));
        printf("Starting I/Q Stream\n");
        IQThreadRunning = 1;
        if(pthread_create(&IQTransmitThread, NULL, IQTransmitLoop, NULL) != 0) {
        	perror("Creating IQ Transmit Thread:");
        	return;
        }
    } else {
    	if(clientAddr == NULL) {
    		fprintf(stderr, "Received stop command while not streaming\n");
    		return;
    	}
    	if(addr->sin_addr.s_addr != clientAddr->sin_addr.s_addr) {
    		fprintf(stderr, "Received Stop command from invalid address\n");
    		return;
    	}
    	IQThreadRunning = 0;
    	free(clientAddr);
    	clientAddr = NULL;
        printf("Stopping I/Q Stream\n");
    }

    if(request->startStop && 0x10 == 0x10) {
        printf("Starting wideband stream\n");
    } else {
        printf("Stopping wideband stream\n");
    }
}

void *IQTransmitLoop() {
	int i, j;
	MetisPacket metisPacket;
	ssize_t bytesWritten;
	unsigned int sequence = 0;
	unsigned char roundRobin = 0;

	//  Prepare MetisPacket attributes that won't change
	metisPacket.header.magic = htons(0xEFFE);
	metisPacket.header.opcode = 0x01;
	metisPacket.header.endpoint = 0x06;

	metisPacket.packets[0].magic[0] = SYNC;
	metisPacket.packets[0].magic[1] = SYNC;
	metisPacket.packets[0].magic[2] = SYNC;
	metisPacket.packets[1].magic[0] = SYNC;
	metisPacket.packets[1].magic[1] = SYNC;
	metisPacket.packets[1].magic[2] = SYNC;


	//  These eventually should rotate
	metisPacket.packets[0].header[0] = 0x0;
	metisPacket.packets[0].header[1] = 0x0;
	metisPacket.packets[0].header[2] = 0x0;
	metisPacket.packets[0].header[3] = 0x0;
	metisPacket.packets[0].header[4] = 0x0;
	metisPacket.packets[1].header[0] = 0x1;
	metisPacket.packets[1].header[0] = 0x0;
	metisPacket.packets[1].header[0] = 0x0;
	metisPacket.packets[1].header[0] = 0x0;
	metisPacket.packets[1].header[0] = 0x0;

	for(j = 0; j < 2; ++j) {
		for(i = 0; i < 63; ++i) {
			memset(&metisPacket.packets[j].samples.in[i].i, 0xFF, 3);
			memset(&metisPacket.packets[j].samples.in[i].q, 0x00, 3);
		}
	}

	printf("Starting Transmit Thread\n");
	while(IQThreadRunning) {
		if(clientAddr == NULL) return NULL;

		// 384k
		// usleep(328);
		// 192k
		usleep(656);

		metisPacket.header.sequence = htonl(sequence++);
		constructHeader(&roundRobin, &metisPacket.packets[0]);
		constructHeader(&roundRobin, &metisPacket.packets[1]);

		//  This is probably a thread issue here.  I should probably pass in clientAddr when the thread is created.
	    bytesWritten = sendto(serviceSocket, &metisPacket, sizeof(metisPacket), 0, (struct sockaddr *) clientAddr, sizeof(struct sockaddr_in));
	    if(bytesWritten == -1) {
	        perror("Sending Data Packet: ");
	        return NULL;
	    }
	}

	printf("Ending transmit thread\n");
	return NULL;
}

void constructHeader(unsigned char *roundRobin, OzyPacket *packet) {
	packet->header[0] = *roundRobin << 3;

	switch(*roundRobin) {
	case 0:
		(*roundRobin)++;
		packet->header[2] = 0x00;
		packet->header[3] = 0x00;
		packet->header[4] = 35;
		break;
	case 1:
		//  Analog in lines
		packet->header[1] = 0x00;
		packet->header[2] = 0x00;
		packet->header[3] = 0x00;
		packet->header[4] = 0x00;
		(*roundRobin)++;
		break;
	case 2:
		//  Analog in lines
		packet->header[1] = 0x00;
		packet->header[2] = 0x00;
		packet->header[3] = 0x00;
		packet->header[4] = 0x00;
		(*roundRobin)++;
		break;
	case 3:
		// Analog in lines
		packet->header[1] = 0x00;
		packet->header[2] = 0x00;
		packet->header[3] = 0x00;
		packet->header[4] = 0x00;
		(*roundRobin)++;
		break;
	case 4:
		packet->header[1] = 35 << 1;
		packet->header[2] = 0x00;
		packet->header[3] = 0x00;
		packet->header[4] = 0x00;
		(*roundRobin) = 0x00;
		break;
	default:
		fprintf(stderr, "Shouldn't ever be here!");
		break;
	}
}

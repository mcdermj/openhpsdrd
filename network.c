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
#include <fcntl.h>

#include "openhpsdr.h"
#include "network.h"

#define SYNC 0x7F

//  XXX These are ugly and should go away mostly.
static struct sockaddr_in *clientAddr = NULL;
static struct sockaddr_in *widebandClientAddr = NULL;
static int IQThreadRunning = 0;
static int WidebandThreadRunning = 0;
static pthread_t IQTransmitThread;
static pthread_t WidebandTransmitThread;
static int serviceSocket;
static int frequencyFile;
static unsigned int frequency = 10000000;

void socketServiceLoop(short port, int txDevice) {
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
    
    if((frequencyFile = open("/sys/class/hpsdr/hpsdrrx0/frequency", O_WRONLY)) == -1) {
    	perror("Frequency file: ");
    }
    
    for(;;) {
    	packetAddressLength = sizeof(packetAddress);
    
        bytesReceived = recvfrom(serviceSocket, &receivedPacket, sizeof(receivedPacket), 0, (struct sockaddr *) &packetAddress, &packetAddressLength);
        if(bytesReceived == -1) {
            //  Need to trap EAGAIN or EWOULDBLOCK
            perror("Receiving packet:");
            exit(1);
        }
        
        if(receivedPacket.header.magic != 0xFEEF) {
        	fprintf(stderr, "Bad magic on packet: 0x%x\n", receivedPacket.header.magic);
        	continue;
        }

        switch(receivedPacket.header.opcode) {
   	    	case 0x01:
				//  Sequence number and endpoint should get checked and 
				//  validated here
				parseOzyPacket(&receivedPacket.packets[0], txDevice);
				parseOzyPacket(&receivedPacket.packets[1], txDevice);
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

void parseOzyPacket(OzyPacket *packet, int txDevice) {
	unsigned int newFrequency;
	
	switch(packet->header[0] >> 1) {
		case 0:
			// Parse out the preamp
			//  We should probably check to see if this has changed
			//  before we go and do a syscall every packet
			if((packet->header[3] & 0x04) > 0) {
			} else {
			}
			break;
		case 1:
			//  Frequency for the transmitter
		case 2:
			//  Frequency for receiver 1
			newFrequency = (packet->header[1] << 24) +
			               (packet->header[2] << 16) +
			               (packet->header[3] << 8) +
			               packet->header[4];
			if(frequency != newFrequency) {
				frequency = newFrequency;
				fprintf(stderr, "Changed frequency to %u\n", frequency);
				dprintf(frequencyFile, "%u", frequency);
			}
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			break;
		default:
			printf("Unknown value of C0 received: %d\n",packet->header[0] >> 1);
			break;
	}
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

    replyPacket.boardid = 0x01;

    memset(&replyPacket.padding, 0x00, sizeof(replyPacket.padding));

    bytesWritten = sendto(serviceSocket, &replyPacket, sizeof(replyPacket), 0, (struct sockaddr *) clientAddr, sizeof(struct sockaddr_in));
    if(bytesWritten == -1) {
        perror("Discovery Reply: ");
        return;
    }

    printf("Sent reply of %d bytes to %s on port %d\n", (int) bytesWritten, ipString, ntohs(clientAddr->sin_port));
}

void startStopHandler(MetisStartStop *request, struct sockaddr_in *addr, int serviceSocket) {
    printf("Entering Start/Stop Handler\n");
    printf("Start/Stop value is %X\n", request->startStop);
    
    /* if(!request->startStop) {
    	fprintf(stderr, "Invalid start/stop packet\n");
    	return;
    } */

    if((request->startStop & 0x01) == 1 && IQThreadRunning == 0) {
        printf("Starting I/Q Stream\n");
        IQThreadRunning = 1;
        if(pthread_create(&IQTransmitThread, NULL, IQTransmitLoop, addr) != 0) {
        	perror("Creating IQ Transmit Thread:");
        	return;
        }
    } else if((request->startStop & 0x01) == 0 && IQThreadRunning == 1) {
    	if(addr->sin_addr.s_addr != clientAddr->sin_addr.s_addr) {
    		fprintf(stderr, "Received Stop command from invalid address\n");
    		return;
    	}
    	IQThreadRunning = 0;
        printf("Stopping I/Q Stream\n");
    }

    if((request->startStop & 0x10) == 1 && WidebandThreadRunning == 0) {
        printf("Starting wideband stream\n");
        
        WidebandThreadRunning = 1;
        if(pthread_create(&WidebandTransmitThread, NULL, WidebandTransmitLoop, addr) != 0) {
        	perror("Creating Wideband Transmit Thread:");
        	return;
        }

    } else if((request->startStop & 0x10) == 0 && WidebandThreadRunning == 1) {
    	if(addr->sin_addr.s_addr != widebandClientAddr->sin_addr.s_addr) {
    		fprintf(stderr, "Received wideband Stop command from invalid address\n");
    		return;
    	}
    	WidebandThreadRunning = 0;

        printf("Stopping wideband stream\n");
    }
}

void *IQTransmitLoop(void *args) {
	int i, j, sampleNum;
	MetisPacket metisPacket;
	ssize_t bytesWritten;
	unsigned int sequence = 0;
	unsigned char roundRobin = 0;
	int receiverDevice = 0;
	int IQSamples[252];
	
	clientAddr = malloc(sizeof(struct sockaddr_in));
    memcpy(clientAddr, args, sizeof(struct sockaddr_in));

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
			metisPacket.packets[j].samples.in[i].mic = 0;
			memset(&metisPacket.packets[j].samples.in[i].i, 0x00, 3);
			memset(&metisPacket.packets[j].samples.in[i].q, 0x00, 3);
		}
	}
	
	if((receiverDevice = open("/dev/hpsdrrx0", O_RDONLY)) == -1) {
		perror("Opening Receiver Device: ");
		return NULL;
	}

	printf("Starting Transmit Thread\n");
	while(IQThreadRunning) {
		if(clientAddr == NULL) break;
		
		// Read 63 I/Q samples from the hardware
		if(read(receiverDevice, IQSamples, sizeof(IQSamples)) == -1) {
			perror("Reading IQ Samples: ");
			continue;
		}
		
		for(j = 0, sampleNum = 0; j < 2; ++j) {
			for(i = 0; i < 63; ++i) {
				//printf("Processing sample %d\n", sampleNum);
				metisPacket.packets[j].samples.in[i].i[2] = IQSamples[sampleNum] & 0xFF;
				metisPacket.packets[j].samples.in[i].i[1] = IQSamples[sampleNum] >> 8 & 0xFF;
				metisPacket.packets[j].samples.in[i].i[0] = IQSamples[sampleNum++] >> 16 & 0xFF;
				
				//printf("Processing sample %d\n", sampleNum);
				metisPacket.packets[j].samples.in[i].q[2] = IQSamples[sampleNum] & 0xFF;
				metisPacket.packets[j].samples.in[i].q[1] = IQSamples[sampleNum] >> 8 & 0xFF;
				metisPacket.packets[j].samples.in[i].q[0] = IQSamples[sampleNum++] >> 16 & 0xFF;
				
				/*memcpy(&metisPacket.packets[j].samples.in[i].i, &IQSamples[sampleNum++], 3);
				memcpy(&metisPacket.packets[j].samples.in[i].q, &IQSamples[sampleNum], 3);*/
			}
		}

		metisPacket.header.sequence = htonl(sequence++);
		constructHeader(&roundRobin, &metisPacket.packets[0]);
		constructHeader(&roundRobin, &metisPacket.packets[1]);

		//  This is probably a thread issue here.  I should probably pass in clientAddr when the thread is created.
	    bytesWritten = sendto(serviceSocket, &metisPacket, sizeof(metisPacket), 0, (struct sockaddr *) clientAddr, sizeof(struct sockaddr_in));
	    if(bytesWritten == -1) {
	        perror("Sending Data Packet: ");
	        break;
	    }
	}
	
	if(close(receiverDevice) == -1) {
		perror("Closing receiver:");
	}
	
	free(clientAddr);
	clientAddr = NULL;

	printf("Ending transmit thread\n");
	return NULL;
}

void *WidebandTransmitLoop(void *args) {
	MetisPacket metisPacket;
	ssize_t bytesWritten;
	unsigned int sequence = 0;
	
	widebandClientAddr = malloc(sizeof(struct sockaddr_in));
    memcpy(widebandClientAddr, args, sizeof(struct sockaddr_in));

	//  Prepare MetisPacket attributes that won't change
	metisPacket.header.magic = htons(0xEFFE);
	metisPacket.header.opcode = 0x01;
	metisPacket.header.endpoint = 0x04;
	
	memset(metisPacket.packets, 0, 1024);

	printf("Starting Wideband Transmit Thread\n");
	while(WidebandThreadRunning) {
		if(widebandClientAddr == NULL) break;
		
		usleep(83);
		
		metisPacket.header.sequence = htonl(sequence++);

		//  This is probably a thread issue here.  I should probably pass in clientAddr when the thread is created.
	    bytesWritten = sendto(serviceSocket, &metisPacket, sizeof(metisPacket), 0, (struct sockaddr *) clientAddr, sizeof(struct sockaddr_in));
	    if(bytesWritten == -1) {
	        perror("Sending Data Packet: ");
	        break;
	    }
	}
	
	free(widebandClientAddr);
	widebandClientAddr = NULL;

	printf("Ending Wideband transmit thread\n");
	return NULL;
}


void constructHeader(unsigned char *roundRobin, OzyPacket *packet) {
	packet->header[0] = *roundRobin << 3;

	switch(*roundRobin) {
		case 0:
			packet->header[2] = 0x00;
			packet->header[3] = 0x00;
			packet->header[4] = 35;
			(*roundRobin)++;
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

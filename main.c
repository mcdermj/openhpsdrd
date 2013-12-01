#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "openhpsdr.h"

void discoveryHandler(MetisDiscoveryRequest *request, struct sockaddr_in *clientAddr, int serviceSocket);
void startStopHandler(MetisStartStop *request, struct sockaddr_in *clientAddr, int serviceSocket);
void usage(int exitcode);

int main(int argc, char **argv) {

    int serviceSocket;
    ssize_t bytesReceived;
    struct sockaddr_in bindAddress;
    struct sockaddr_in packetAddress;
    int packetAddressLength;
    MetisPacket receivedPacket;
    const int one = 1;
    short port = 1024;
    short int_arg = 0;
    int option;

    //  Parse arguments
    while((option = getopt(argc, argv, ":p:h")) != -1) {
    	switch(option) {
    	case 'h':
    		usage(0);
    		break;
    	case 'p':
    		int_arg = (short) atoi(optarg);

    		if(int_arg == 0 || errno == ERANGE) {
    			fprintf(stderr, "Invalid port number: %s\n", optarg);
    			exit(1);
    		}

    		port = int_arg;
    		break;
    	case '?':
    		fprintf(stderr, "Unknown option %c\n", optopt);
    		usage(1);
    		break;
    	case ':':
    		fprintf(stderr, "Missing option %c\n", optopt);
    		usage(1);
    		break;
    	default:
    		fprintf(stderr, "We shouldn't ever be here: %c\n", option);
    		usage(1);
    		break;
    	}
    }

    if((serviceSocket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Service Socket: ");
        exit(0);
    }

    bindAddress.sin_family = AF_INET;
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    //  This should be a settable parameter
    bindAddress.sin_port = htons(port);

    //  Set some socket options
    if(setsockopt(serviceSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1) {
        perror("Setting REUSEADDR: ");
        exit(0);
    }

    if(bind(serviceSocket, (struct sockaddr *) &bindAddress, sizeof(bindAddress)) == -1) {
        perror("Bind Socket: ");
        error(0);
    }

    for(;;) {
        bytesReceived = recvfrom(serviceSocket, &receivedPacket, sizeof(receivedPacket), 0, (struct sockaddr *) &packetAddress, &packetAddressLength);
        if(bytesReceived == -1) {
            //  Need to trap EAGAIN or EWOULDBLOCK
            perror("Receiving packet:");
            exit(0);
        }

        switch(receivedPacket.opcode) {
            case 0x02:
                discoveryHandler((MetisDiscoveryRequest *) &receivedPacket, &packetAddress, serviceSocket);
                break;
            case 0x04:
                startStopHandler((MetisStartStop *) &receivedPacket, &packetAddress, serviceSocket);
                break;
            case 0x01:
                // printf("Data packet received\n");
                break;
            default:
                printf("Unknown pakcet received\n");
                break;
        }
    }
    close(serviceSocket);
}

void discoveryHandler(MetisDiscoveryRequest *request, struct sockaddr_in *clientAddr, int serviceSocket) {
    char ipString[16];
    MetisDiscoveryReply replyPacket;
    struct ifreq buffer;
    int i;
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

void startStopHandler(MetisStartStop *request, struct sockaddr_in *clientAddr, int serviceSocket) {
    printf("Entering Start/Stop Handler\n");

    if(request->startStop && 0x01 == 0x01) {
        printf("Starting I/Q Stream\n");
    } else {
        printf("Stopping I/Q Stream\n");
    }

    if(request->startStop && 0x10 == 0x10) {
        printf("Starting wideband stream\n");
    } else {
        printf("Stopping wideband stream\n");
    }
}

void usage(int exitcode) {
	fprintf(stderr, "Usage: openhpsdrd [-p port]\n");
	exit(exitcode);
}


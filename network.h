/*
 * network.h
 *
 *  Created on: Nov 30, 2013
 *      Author: mcdermj
 */

#ifndef NETWORK_H_
#define NETWORK_H_

void discoveryHandler(MetisDiscoveryRequest *request, struct sockaddr_in *clientAddr, int serviceSocket);
void startStopHandler(MetisStartStop *request, struct sockaddr_in *clientAddr, int serviceSocket);
void socketServiceLoop(short port, int txDevice);
void *IQTransmitLoop(void *);
void *WidebandTransmitLoop(void *);
void constructHeader(unsigned char *roundRobin, OzyPacket *packet);
void parseOzyPacket(OzyPacket *packet, int txDevice);

#endif /* NETWORK_H_ */

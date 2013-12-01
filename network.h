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
void socketServiceLoop(short port);
void *IQTransmitLoop();

#endif /* NETWORK_H_ */

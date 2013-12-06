#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "openhpsdr.h"
#include "network.h"

void usage(int exitcode);

int main(int argc, char **argv) {
    short port = 1024;
    short int_arg = 0;
    int option;
    int txDevice;

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

    //  Open the transmitter control file
    if((txDevice = open("/dev/hpsdrtx", O_WRONLY)) == -1) {
        perror("Couldn't open /dev/hpsdrtx\n");
	exit(1);
    }

    socketServiceLoop(port, txDevice);

    return(0);
}

void usage(int exitcode) {
	fprintf(stderr, "Usage: openhpsdrd [-p port]\n");
	exit(exitcode);
}


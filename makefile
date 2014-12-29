#
# Makefile for openhpsdrd
# 
#

C_SRC := main.c network.c
CFLAGS := -g -O0 -Werror -Wall
LDFLAGS := -lpthread

CROSS_COMPILE := armv7l-unknown-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
NM := $(CROSS_COMPILE)nm

ELF = openhpsdrd
OBJ := $(patsubst %.c,%.o,$(C_SRC))

.PHONY: all
all: $(ELF)
	
.PHONY:
clean:
	$(RM) $(ELF) $(OBJ) *.objdump *.map

#$(OBJ): %.o: %.c
#%.o: %.c
#	$(CC) $(CFLAGS) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
.c.o:

$(ELF): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

testdevice: testdevice.o
	$(CC) $(CFLAGS) testdevice.o -o $@ $(LDFLAGS)


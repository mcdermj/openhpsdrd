#
# Copyright Altera 2013
# All Rights Reserved.
#

C_SRC := main.c network.c
CFLAGS := -g -O0 -Werror -Wall
LDFLAGS := -lpthread

CROSS_COMPILE := arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
NM := $(CROSS_COMPILE)nm

ifeq ($(or $(COMSPEC),$(ComSpec)),)
RM := rm -rf
else
RM := cs-rm -rf
endif

#ELF ?= $(basename $(firstword $(C_SRC)))
ELF = openhpsdrd
OBJ := $(patsubst %.c,%.o,$(C_SRC))

.PHONY: all
all: $(ELF)
	
.PHONY:
clean:
	$(RM) $(ELF) $(OBJ) *.objdump *.map

$(OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#$(ELF): $(OBJ)
#	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)
#	$(NM) $@ > $@.map

$(ELF): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)
	$(NM) $@ > $@.map
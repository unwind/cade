#
# Makefile for building the tests for CADE.
#
# Written by Emil Brink <emil@obsession.se>, April 2012.
#

CADE=../cade
CADE_C=$(CADE).c
CADE_H=$(CADE).h

CFLAGS=-I$(dir $(CADE)) 

.PHONY:	clean

ALL	= test

ALL:	$(ALL)

# ---------------------------------------------- TARGETS

test:	test.c $(CADE_C) $(CADE_H)
	gcc $(CFLAGS) -o test test.c $(CADE_C)

# ---------------------------------------------- MAINTENANCE

clean:
	rm -f $(ALL)

#
# Makefile for CADE, a DCPU-16 emulator.
#

CFLAGS=-Wall -DCADE_STANDALONE


.PHONY:	clean doc

ALL	= cade

ALL:	$(ALL)

# ---------------------------------------------- TARGETS

cade:	cade.c cade.h

# ---------------------------------------------- MAINTENANCE

clean:
	rm -f $(ALL)

doc:
	doxygen Doxyfile

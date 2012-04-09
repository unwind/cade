#
#
#

CFLAGS=-Wall -DCADE_STANDALONE


.PHONY:	clean

ALL	= cade

ALL:	$(ALL)

cade:	cade.c cade.h

# ---------------------------------------------- MAINTENANCE

clean:
	rm -f $(ALL)

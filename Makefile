#
#
#

CFLAGS=-Wall

.PHONY:	clean

ALL	= cade

ALL:	$(ALL)

cade:	cade.c

# ---------------------------------------------- MAINTENANCE

clean:
	rm -f $(ALL)

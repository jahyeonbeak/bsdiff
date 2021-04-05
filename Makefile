CC=gcc
LDFLAGS=
CFLAGS        +=    -O3 -lbz2

PREFIX        ?=    /usr/local
INSTALL_PROGRAM    ?=    cp
INSTALL_MAN    ?=    cp

all:        bsdiff bspatch
bsdiff:        bsdiff_main.c
	$(CC) bsdiff.c bsdiff_main.c $(CFLAGS) $(LDFLAGS) -o  bsdiff
bspatch:    bspatch_main.c
	$(CC) bspatch_main.c  $(CFLAGS) $(LDFLAGS) -o  bspatch
install:
	${INSTALL_PROGRAM} bsdiff bspatch ${PREFIX}/bin
	${INSTALL_MAN} bsdiff.1 bspatch.1 ${PREFIX}/man/man1

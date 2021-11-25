CC      ?= gcc
CFLAGS  ?= -Wall
INCLUDES ?= -I include/

all: cassini saturnd

cassini: cassini.o timing-text-io.o helpers.o
	$(CC) $(CFLAGS) -o cassini cassini.o timing-text-io.o helpers.o

timing-text-io.o: src/timing-text-io.c
	$(CC) $(CFLAGS) $(INCLUDES) -c src/timing-text-io.c -o timing-text-io.o

helpers.o: src/helpers.c
	$(CC) $(CFLAGS) $(INCLUDES) -c src/helpers.c -o helpers.o

cassini.o: src/cassini.c
	$(CC) $(CFLAGS) $(INCLUDES) -c src/cassini.c -o cassini.o

saturnd: saturnd.o
	$(CC) $(CFLAGS) -o saturnd saturnd.o

saturnd.o: src/saturnd.c
	$(CC) $(CFLAGS) $(INCLUDES) -c src/saturnd.c -o saturnd.o

distclean:
	$(RM) cassini saturnd *.o

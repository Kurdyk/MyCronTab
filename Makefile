CC      ?= gcc
CFLAGS  ?= -Wall
INCLUDES ?= -I include/

all: cassini saturnd

cassini: cassini.o timing-text-io.o helpers.o
	$(CC) $(CFLAGS) -o cassini _build/cassini.o _build/timing-text-io.o _build/helpers.o

%.o: src/%.c
	mkdir -p _build/
	@$(CC) $(INCLUDES) -c $< -o _build/$@

saturnd: saturnd.o
	$(CC) $(CFLAGS) -o saturnd _build/saturnd.o _build/timing-text-io.o _build/helpers.o

distclean:
	$(RM) -r cassini saturnd *.o _build/
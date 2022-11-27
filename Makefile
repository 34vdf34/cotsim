CC=gcc

EXTRA_WARNINGS = -Wall 

INC=`pkg-config --cflags libxml-2.0`

LIBS = -lcurl -lxml2 -lsqlite3

CFLAGS=-ggdb $(EXTRA_WARNINGS)

BINS=cotsim

all: cotsim

cotsim: cotsim.c log.c ini.c
	 $(CC) $+ $(CFLAGS) $(LIBS) -o $@ -I.

clean:
	rm -rf $(BINS)
	rm *.o
	

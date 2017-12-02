CC=gcc
MAK=Makefile
RM=rm


CFLAGS=-g -Wall -fPIC -O3

DYNLIBPRE=lib
UNAME=$(shell uname)
ifeq ($(UNAME),Linux)
    DYNLIBEXT=.so
else ifeq ($(UNAME),SunOS)
    DYNLIBEXT=.so
else ifeq ($(UNAME),Darwin)
    DYNLIBEXT=.dylib
else
    $(error Unknown system!)
endif
ifeq ($(UNAME),Linux)
    LIBDIR=/usr/local/lib
endif

.PHONY : all clean

all : $(DYNLIBPRE)jhjr_math$(DYNLIBEXT)

clean :
	$(RM) -f *.o *$(DYNLIBEXT)

$(DYNLIBPRE)jhjr_math$(DYNLIBEXT) : jhjr_math.c jhjr_math.h
	$(CC) $(CFLAGS) -shared -o $(DYNLIBPRE)jhjr_math$(DYNLIBEXT) jhjr_math.c

install : $(DYNLIBPRE)jhjr_math$(DYNLIBEXT)
	sudo cp -f $(DYNLIBPRE)jhjr_math$(DYNLIBEXT) $(LIBDIR)

CC   = gcc
MAK  = Makefile
RM   = rm

JHJrMATH = libjhjr_math.so
JHJrMATH_DIR = jhjr_math/

CFLAGS  = -g -I$(JHJrMATH_DIR) -O0
LFLAGS  = -L$(JHJrMATH_DIR)
LIB  = -lpthread
LIB += -ljhjr_math

BIN  = 3sat


.PHONY : all clean clobber

all : $(BIN)

clean :
	$(RM) -f $(BIN) *.o

clobber :
	make clean
	make -C $(JHJrMATH_DIR) clean

$(BIN) : 3sat.o $(JHJrMATH) $(MAK)
	$(CC) -o $(BIN) 3sat.o $(LFLAGS) $(LIB)

3sat.o : 3sat.c $(MAK)
	$(CC) -Wall -c -o 3sat.o 3sat.c $(CFLAGS)

$(JHJrMATH) :
	make -C $(JHJrMATH_DIR)

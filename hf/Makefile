# Makefile for HF layer
LIB	= hf
INCDIR	= ${MINIREL_HOME}/h
INCS	= hfinternal.h hfHash.h
SRCS	= hf.c hfinternal.c hfHash.c
TESTS	= hftest.c
OBJS	= ${SRCS:.c=.o}
LIBS	= lib${LIB}.a ../pf/libpf.a ../bf/libbf.a

#############################################################################
# This macro definition can be overwritten by command-line definitions.
CC	= gcc
CFLAGS	= -g -ansi -pedantic -Wall -no-pie
#CFLAGS	= -O -ansi -pedantic
#############################################################################

all: lib${LIB}.a ${LIB}test

${LIB}test: ${LIB}test.o lib${LIB}.a
	$(CC) $(CFLAGS) -o $@ $< ${LIBS} -lm

lib${LIB}.a: $(OBJS)
	ar cr lib${LIB}.a $(OBJS)
	ranlib lib${LIB}.a

clean:
	rm -f lib${LIB}.a *.o ${LIB}test *.bak *~

.c.o:; $(CC) $(CFLAGS) -c $< -I. -I$(INCDIR)


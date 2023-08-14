CC = gcc
DBG = #-g 
OPT = -O2
CFLAGS = $(DBG) -Wall -ansi -pedantic -m32 -std=c99

.SILENT:

hd: hd.c Makefile
	$(CC) $(CFLAGS) -o $@ $<
	
clean: Makefile
	rm -rf Debug Release hd


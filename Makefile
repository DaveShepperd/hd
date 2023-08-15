CC = gcc

DBG = -g 	#include debugging just because
OPT = -O2	#No reason to not optimize
MACH = -m32	#have to build 32 bit on Windows, so do it in Linux too
STD = #-std=c99  #let is use the latest std
WARN = -Wall
STRICT = -ansi #-pedantic #Cannot use pedantic due to the 'll' constructs in formats.
CFLAGS = $(DBG) $(OPT) $(MACH) $(STD) $(WARN) $(STRICT)

.SILENT:

hd: hd.c Makefile
	$(CC) $(CFLAGS) -o $@ $<
	
clean: Makefile
	rm -rf Debug Release hd


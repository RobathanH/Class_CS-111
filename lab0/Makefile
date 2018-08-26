#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0


#compiler
CC = gcc

#compiler flags:
CFLAGS = -g -Wall -Wextra

all: lab0

lab0: lab0.c
	$(CC) $(CFLAGS) -o lab0 lab0.c

#This check will return nothing if the output is correct. Otherwise it will show differences in output and the 'make check' command will experience an error
check: lab0 test correct.txt
	@./test 2>&1 | diff correct.txt -

dist: lab0.c Makefile backtrace.png breakpoint.png README
	tar -czvf lab0-904836501.tar.gz lab0.c Makefile backtrace.png breakpoint.png README test correct.txt

clean:
	rm lab0 lab0-904836501.tar.gz

#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0

CC = gcc
CFLAGS = -g -Wall -Wextra

all: lab1a

lab1a: lab1a.c
	$(CC) $(CFLAGS) -o lab1a lab1a.c

dist: lab1a.c README Makefile
	tar -czvf lab1a-904836501.tar.gz lab1a.c Makefile README

clean:
	rm lab1a lab1a-904836501.tar.gz

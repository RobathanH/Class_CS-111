#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0

CC = gcc
CFLAGS = -g -Wall -Wextra

all: lab1b-client lab1b-server

lab1b-client: lab1b-client.c
	$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c -lz

lab1b-server: lab1b-server.c
	$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c -lz

dist: lab1b-server.c lab1b-client.c README Makefile
	tar -czvf lab1b-904836501.tar.gz lab1b-server.c lab1b-client.c README Makefile

clean: lab1b-client lab1b-server dist
	rm lab1b-client lab1b-server lab1b-904836501.tar.gz

#NAME: Robathan Harries
#ID: 904836501
#EMAIL: r0b@ucla.edu
#SLIPDAYS: 0

CC = gcc
CFLAGS = -Wall -Wextra -lm -lmraa -lssl -lcrypto -g


all: lab4c_tcp lab4c_tls


lab4c_tcp: lab4c_tcp.c
	@$(CC) $(CFLAGS) -o lab4c_tcp lab4c_tcp.c


lab4c_tls: lab4c_tls.c
	@$(CC) $(CFLAGS) -o lab4c_tls lab4c_tls.c


dist: lab4c_tcp.c lab4c_tls.c README
	@tar -czvf lab4c-904836501.tar.gz lab4c_tcp.c lab4c_tls.c README Makefile


clean:
	@rm -f lab4c_tcp lab4c_tls lab4c-904836501.tar.gz

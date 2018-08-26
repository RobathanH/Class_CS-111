#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0

CC = gcc
CFLAGS = -Wall -Wextra -lm


all: lab3a


lab3a: lab3a.c ext2_fs.h
	@$(CC) $(CFLAGS) -o lab3a lab3a.c


dist: lab3a.c ext2_fs.h Makefile README
	@tar -czvf lab3a-904836501.tar.gz lab3a.c ext2_fs.h Makefile README


clean:
	@rm -f lab3a lab3a-904836501.tar.gz

#NAME: Robathan Harries
#ID: 904836501
#EMAIL: r0b@ucla.edu
#SLIPDAYS: 0


CC = gcc
CFLAGS = -lm -lmraa -Wall -Wextra


all: lab4b


lab4b: lab4b.c
	@$(CC) $(CFLAGS) -o lab4b lab4b.c


check: lab4b test.txt
	@./lab4b <test.txt 2>&1 >/dev/nill


clean:
	@rm -f lab4b lab4b-904836501.tar.gz


dist: lab4b.c README test.txt
	@tar -czvf lab4b-904836501.tar.gz lab4b.c test.txt Makefile README

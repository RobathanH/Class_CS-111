#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0

CC = gcc
CFLAGS = -g -Wall -Wextra -pthread


all: build

build: lab2_add lab2_list

lab2_add: lab2_add.c
	$(CC) -o lab2_add $(CFLAGS) lab2_add.c

lab2_list: lab2_list.c SortedList.h SortedList.c
	$(CC) -o lab2_list $(CFLAGS) lab2_list.c SortedList.c

tests: lab2_add lab2_list add_test.sh list_test.sh
	rm -f lab2_add.csv
	rm -f lab2_list.csv
	./add_test.sh
	./list_test.sh

graphs: lab2_add.gp lab2_list.gp
	./lab2_add.gp
	./lab2_list.gp

dist: lab2_add.c SortedList.h SortedList.c lab2_list.c Makefile README tests graphs
	tar -czvf lab2a-904836501.tar.gz lab2_add.c SortedList.h SortedList.c lab2_list.c Makefile lab2_add.csv lab2_list.csv lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png lab2_add-5.png lab2_list-1.png lab2_list-2.png lab2_list-3.png lab2_list-4.png lab2_add.gp lab2_list.gp README add_test.sh list_test.sh

clean:
	rm -f lab2_add lab2_list lab2a-904836501.tar.gz

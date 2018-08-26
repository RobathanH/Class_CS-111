#NAME: Robathan Harries
#EMAIL: r0b@ucla.edu
#ID: 904836501
#SLIPDAYS: 0

CC = gcc
CFLAGS = -g -Wall -Wextra -pthread

#Iteration amount is increased so that profile can do enough samples
PROFILE_TEST = ./lab2_list --threads=12 --iter=1000 --sync=s
PROFILE_LIB = /usr/lib/libprofiler.so

#The sampling frequency was too low to give much useful information, so this environment variable change increases it
FREQ_ADJ = CPUPROFILE_FREQUENCY=1000

#Where the raw profiling output is stored
RAW_FILE = raw.prof

all: lab2_list


lab2_list: lab2_list.c SortedList.c SortedList.h
	@$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c


tests: list_test.sh lab2_list
	@./list_test.sh


profile: lab2_list
	@rm -f profile.out $(RAW_FILE)
	@env $(FREQ_ADJ) CPUPROFILE=$(RAW_FILE) LD_PRELOAD=$(PROFILE_LIB) $(PROFILE_TEST)
	@google-pprof --text lab2_list $(RAW_FILE) >> profile.out
	@google-pprof --list=threadHandler lab2_list $(RAW_FILE) >> profile.out
	@rm -f $(RAW_FILE)


graphs: list_graph.gp lab2b_list.csv
	@rm -f lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png
	@./list_graph.gp


dist: SortedList.h SortedList.c lab2_list.c Makefile README profile tests graphs
	@tar -czvf lab2b-904836501.tar.gz SortedList.h SortedList.c lab2_list.c Makefile README lab2b_list.csv profile.out lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png list_test.sh list_graph.gp


clean:
	@rm -f lab2_list lab2b-904836501.tar.gz

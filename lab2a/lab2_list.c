//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0

#include "SortedList.h"
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>

//Global locks
pthread_mutex_t mutex, errorMutex;//errorMutex is only used when executing an emergency error after a thread discovers that the list is corrupted. Otherwise some objects will be double freed
volatile int lock;
int opt_yield;

void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab2_list [--threads=#] [--iterations=#] [--yield=[idl]] [--sync=[m,s]]\n");
  exit(1);
}

void formatErrBadNum() {
  fprintf(stderr, "Incorrect Format: argument values must be nonzero positive integers.\n");
  exit(1);
}

void formatErrBadYield() {
  fprintf(stderr, "Incorrect Format: Argument for --yield option must be some combination of i, d, and l.\n");
  exit(1);
}

void formatErrBadSync() {
  fprintf(stderr, "Incorrect Format: Argument for --sync option must be one of [s, m].\n");
  exit(1);
}

void optionParser(int argc, char **argv, int *threadNumPtr, int *iterNumPtr, char *syncOptPtr) {
  //Unset Values - so function can tell whether the option has been set yet
  *threadNumPtr = -1;
  *iterNumPtr = -1;
  opt_yield = 0;
  *syncOptPtr = 0;

  //getopt setup
  char *optstring = "";
  int *longindex = NULL;

  struct option longopts[5];//4 options plus null termination entry
  longopts[0].name = "threads";
  longopts[0].has_arg = 1;
  longopts[0].flag = NULL;
  longopts[0].val = 't';
  longopts[1].name = "iterations";
  longopts[1].has_arg = 1;
  longopts[1].flag = NULL;
  longopts[1].val = 'i';
  longopts[2].name = "yield";
  longopts[2].has_arg = 1;
  longopts[2].flag = NULL;
  longopts[2].val = 'y';
  longopts[3].name = "sync";
  longopts[3].has_arg = 1;
  longopts[3].flag = NULL;
  longopts[3].val = 's';
  longopts[4].name = NULL;
  longopts[4].has_arg = 0;
  longopts[4].flag = NULL;
  longopts[4].val = 0;

  opterr = 0;

  int val;
  while (-1 != (val = getopt_long(argc, argv, optstring, longopts, longindex))) {
    int num;
    switch(val) {
    case 't':
      if (*threadNumPtr != -1) formatErr(); // If the option has already been used

      //Check if argument is integer
      num = atoi(optarg);
      if (num < 0 || (num == 0 && optarg[0] != '0')) formatErrBadNum();

      *threadNumPtr = num;
      break;

    case 'i':
      if (*iterNumPtr != -1) formatErr();

      //check if arg is valid integer
      num = atoi(optarg);
      if (num < 0 || (num == 0 && optarg[0] != '0')) formatErrBadNum();

      *iterNumPtr = num;
      break;

    case 'y':
      if (opt_yield != 0) formatErr();

      int i;
      for (i = 0; i < (int) strlen(optarg); i++) {
	switch(optarg[i]) {
	case 'i':
	  if (opt_yield & INSERT_YIELD) formatErrBadYield();
	  opt_yield += INSERT_YIELD;
	  break;

	case 'd':
	  if (opt_yield & DELETE_YIELD) formatErrBadYield();
	  opt_yield += DELETE_YIELD;
	  break;

	case 'l':
	  if (opt_yield & LOOKUP_YIELD) formatErrBadYield();
	  opt_yield += LOOKUP_YIELD;
	  break;

	default:
	  formatErrBadYield();
	}
      }
      break;

    case 's':
      if (*syncOptPtr != 0) formatErr();

      if (strlen(optarg) != 1 || (optarg[0] != 'm' && optarg[0] != 's')) formatErrBadSync();

      *syncOptPtr = optarg[0];
      break;

    default:
      formatErr();
    }
  }

  //Apply default values if arguments haven't been given
  if (*threadNumPtr == -1) *threadNumPtr = 1;
  if (*iterNumPtr == -1) *iterNumPtr = 1;
}

//Thread arg structure

struct threadArgs {
  SortedList_t *head;
  SortedListElement_t *elementArr;
  int arrLen;
  char syncOpt;

  //Info so that in case of inconsistency the thread can free all allocated memory before exiting
  SortedListElement_t *elementArrStart;
  pthread_t* threadIDs;
  char *keyArrStart;
  struct threadArgs *args;
};

//Error logger
void corruptedList(SortedListElement_t *elementArrStart, pthread_t *threadIDs, char *keyArrStart, struct threadArgs *args) {
  pthread_mutex_lock(&errorMutex);
  
  fprintf(stderr, "List corrupted!\n");
  free(elementArrStart);
  free(threadIDs);
  free(keyArrStart);
  free(args);

  exit(2);
}

//Segmentation fault handler
void segHandler() {
  corruptedList(NULL, NULL, NULL, NULL);
}

void *threadHandler(void *args) {
  struct threadArgs *newArgs = (struct threadArgs *) args;

  int i;

  if (newArgs->syncOpt == 'm') {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      pthread_mutex_lock(&mutex);
      SortedList_insert(newArgs->head, &(newArgs->elementArr[i]));
      pthread_mutex_unlock(&mutex);
    }

    // get list length
    pthread_mutex_lock(&mutex);
    if (-1 == SortedList_length(newArgs->head)) {
      fprintf(stderr, "Len TAG!\n");
      corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
    }
    pthread_mutex_unlock(&mutex);
    
    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      //Lookup element
      pthread_mutex_lock(&mutex);
      SortedListElement_t *target = SortedList_lookup(newArgs->head, newArgs->elementArr[i].key);
      if (target == NULL) {
	fprintf(stderr, "Search TAG!\n");
	corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
      }

      //Delete target
      if (1 == SortedList_delete(target)) {
	fprintf(stderr, "Delete TAG!\n");
	corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
      }
      pthread_mutex_unlock(&mutex);
    }
  }

  else if (newArgs->syncOpt == 's') {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      while (__sync_lock_test_and_set(&lock, 1));
      SortedList_insert(newArgs->head, &(newArgs->elementArr[i]));
      __sync_lock_release(&lock);
    }

    // get list length
    while (__sync_lock_test_and_set(&lock, 1));
    if (-1 == SortedList_length(newArgs->head)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
    __sync_lock_release(&lock);
    
    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      //Lookup element
      while (__sync_lock_test_and_set(&lock, 1));
      SortedListElement_t *target = SortedList_lookup(newArgs->head, newArgs->elementArr[i].key);
      if (target == NULL) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);

      //Delete target
      if (1 == SortedList_delete(target)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
      __sync_lock_release(&lock);
    }
  }

  else {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      SortedList_insert(newArgs->head, &(newArgs->elementArr[i]));
    }

    // get list length
    if (-1 == SortedList_length(newArgs->head)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
    
    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      //Lookup element
      SortedListElement_t *target = SortedList_lookup(newArgs->head, newArgs->elementArr[i].key);
      if (target == NULL) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
      
      //Delete target
      if (1 == SortedList_delete(target)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args);
    }
  }

  return NULL;
}

int main(int argc, char **argv) {

  //Adjustable Constants
  int keyLen = 20;//Number of chars reserved for each key string (including null-terminator)

  //Option parsing
  int threadNum, iterNum;
  char syncOpt;
  optionParser(argc, argv, &threadNum, &iterNum, &syncOpt);

  //Initialize locks/mutex
  lock = 0;
  if (0 != pthread_mutex_init(&mutex, NULL) || 0 != pthread_mutex_init(&errorMutex, NULL)) {
    fprintf(stderr, "Error initializing mutex.\n");
    exit(1);
  }

  //Create shared head of list
  SortedList_t head;
  head.next = &head;
  head.prev = &head;
  head.key = NULL;

  //Set up segfault handler
  signal(SIGSEGV, segHandler);
  
  //Allocate mem for elements, thread IDs, keys, and arg structures
  SortedListElement_t *elements = malloc(threadNum * iterNum * sizeof(SortedListElement_t));
  pthread_t *threads = malloc(threadNum * sizeof(pthread_t));
  char *keys = malloc(threadNum * iterNum * keyLen * sizeof(char));
  int i;
  for (i = 0; i < threadNum * iterNum; i++) {
    keys[i * keyLen + keyLen - 1] = '\0';
    //Leave the rest of the keys undefined, should be effectively random

    elements[i].key = &keys[i];
  }
  struct threadArgs *args = malloc(threadNum * sizeof(struct threadArgs));
  for (i = 0; i < threadNum; i++) {
    args[i].head = &head;
    args[i].elementArr = &elements[i * iterNum];
    args[i].arrLen = iterNum;
    args[i].syncOpt = syncOpt;
    args[i].elementArrStart = elements;
    args[i].threadIDs = threads;
    args[i].keyArrStart = keys;
    args[i].args = args;
  }

  
  //Get time
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);
  
  //Run threads
  for (i = 0; i < threadNum; i++) {
    if (0 != pthread_create(&threads[i], NULL, &threadHandler, (void *) &args[i])) {
      pthread_mutex_lock(&errorMutex);

      fprintf(stderr, "Error creating threads.\n");
      free(elements);
      free(threads);
      free(keys);
      free(args);
      exit(1);
    }
  }
  

  //Join threads
  for (i = 0; i < threadNum; i++) {
    if (0 != pthread_join(threads[i], NULL)) {
      pthread_mutex_lock(&errorMutex);
      
      fprintf(stderr, "Error joining threads.\n");
      free(elements);
      free(threads);
      free(keys);
      free(args);
      exit(1);
    }
  }
  

  //Get time
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);
  
  
  //Check if length is 0
  if (0 != SortedList_length(&head)) {
    pthread_mutex_lock(&errorMutex);
    
    fprintf(stderr, "Non-zero length of final list!\n");
    free(elements);
    free(threads);
    free(keys);
    free(args);
    exit(1);
  }

  //Free memory (thread IDs and elements)
  free(elements);
  free(threads);
  free(keys);
  free(args);
  

  //Print out a successful return if possible
  time_t secDif = end.tv_sec - start.tv_sec;
  long nsecDif = end.tv_nsec - start.tv_nsec;
  nsecDif += secDif * 1e9;

  int opNum = threadNum * iterNum * 3;
  int aveTime = nsecDif / opNum;
  char *yieldopts1, *yieldopts2, *yieldopts3, *syncopts;
  
  if (opt_yield & INSERT_YIELD) yieldopts1 = "i";
  else yieldopts1 = "";

  if (opt_yield & DELETE_YIELD) yieldopts2 = "d";
  else yieldopts2 = "";

  if (opt_yield & LOOKUP_YIELD) yieldopts3 = "l";
  else yieldopts3 = "";

  if (strlen(yieldopts1) == 0 && strlen(yieldopts2) == 0 && strlen(yieldopts3) == 0) yieldopts1 = "none";

  if (syncOpt == 0) syncopts = "none";
  else if (syncOpt == 'm') syncopts = "m";
  else syncopts = "s";

  printf("list-%s%s%s-%s,%i,%i,1,%i,%ld,%i\n", yieldopts1, yieldopts2, yieldopts3, syncopts, threadNum, iterNum, opNum, nsecDif, aveTime);

  exit(0);
}

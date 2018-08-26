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
#include <limits.h>


//Global locks
pthread_mutex_t *mutex, errorMutex;//errorMutex is only used when executing an emergency error after a thread discovers that the list is corrupted. Otherwise some objects will be double freed
volatile int *lock;
int opt_yield;

void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab2_list [--threads=#] [--iterations=#] [--lists=#] [--yield=[idl]] [--sync=[m,s]]\n");
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

void optionParser(int argc, char **argv, int *threadNumPtr, int *iterNumPtr, char *syncOptPtr, int *listNumPtr) {
  //Unset Values - so function can tell whether the option has been set yet
  *threadNumPtr = -1;
  *iterNumPtr = -1;
  opt_yield = 0;
  *syncOptPtr = 0;
  *listNumPtr = -1;

  //getopt setup
  char *optstring = "";
  int *longindex = NULL;

  struct option longopts[6];//5 options plus null termination entry
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
  longopts[4].name = "lists";
  longopts[4].has_arg = 1;
  longopts[4].flag = NULL;
  longopts[4].val = 'l';
  longopts[5].name = NULL;
  longopts[5].has_arg = 0;
  longopts[5].flag = NULL;
  longopts[5].val = 0;

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

    case 'l':
      if (*listNumPtr != -1) formatErr();

      //check if arg is valid integer
      num = atoi(optarg);
      if (num < 0 || (num == 0 && optarg[0] != '0')) formatErrBadNum();

      *listNumPtr = num;
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
  if (*listNumPtr == -1) *listNumPtr = 1;
}

//Thread arg structure

struct threadArgs {
  SortedList_t *heads;
  SortedListElement_t *elementArr;
  int arrLen;
  char syncOpt;
  long *waitTimePtr;
  int listNum;

  //Info so that in case of inconsistency the thread can free all allocated memory before exiting
  SortedListElement_t *elementArrStart;
  pthread_t* threadIDs;
  char *keyArrStart;
  struct threadArgs *args;
  long *waitTimeStart;
  pthread_mutex_t *mutexStart;
  int *lockStart;
};

//Error logger
void corruptedList(SortedListElement_t *elementArrStart, pthread_t *threadIDs, char *keyArrStart, struct threadArgs *args, long *waitTimes, pthread_mutex_t *mutexStart, volatile int *lockStart, SortedList_t *heads) {
  pthread_mutex_lock(&errorMutex);
  
  fprintf(stderr, "List corrupted!\n");
  free(elementArrStart);
  free(threadIDs);
  free(keyArrStart);
  free(args);
  free(waitTimes);
  free(mutexStart);
  free((int*) lockStart);
  free(heads);

  exit(2);
}

//Segmentation fault handler
void segHandler() {
  corruptedList(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

void addTimeDif(const struct timespec *before, const struct timespec *after, struct timespec *total) {
  total->tv_sec += after->tv_sec - before->tv_sec;
  total->tv_nsec += after->tv_nsec - before->tv_nsec;
}

//Hash function to decide which list a string goes to - similar to djb2 hash function
int listIndex(const char *key, int listNum) {
  int result = 0;
  int i = 0;
  while (key[i] != '\0') {
    result = (result + (int) key[i]) % listNum;
    i++;
  }

  return result;
}

//the lockTime pointer should always be set to &total.tv_nsec, which will be incremented by the time it takes to get through locks in this method 
int getTotalLength(SortedList_t *heads, int listNum, char syncOpt, long *lockTime) {
  struct timespec before, after;
  long totalTime = 0;

  int i;
  int total = 0;
  for (i = 0; i < listNum; i++) {
    clock_gettime(CLOCK_REALTIME, &before);
    switch(syncOpt) {
    case 's':
      while (__sync_lock_test_and_set(&lock[i], 1));
      break;
    case 'm':
      pthread_mutex_lock(&mutex[i]);
    }
    clock_gettime(CLOCK_REALTIME, &after);

    int val = SortedList_length(&heads[i]);

    switch(syncOpt) {
    case 's':
      __sync_lock_release(&lock[i]);
      break;
    case 'm':
      pthread_mutex_unlock(&mutex[i]);
    }

    //Find time dif
    if (syncOpt == 'm' || syncOpt == 's')
      totalTime += after.tv_nsec - before.tv_nsec + 1e9 * (after.tv_sec - before.tv_sec);
    
    if (val == -1) {
      if (syncOpt == 'm' || syncOpt == 's')
	*lockTime += totalTime;
      return -1;
    }
    
    total += val;
  }

  //Update lockTime variable
  if (syncOpt == 'm' || syncOpt == 's')
    *lockTime += totalTime;
  
  return total;
}

void *threadHandler(void *args) {
  struct threadArgs *newArgs = (struct threadArgs *) args;

  int i;

  struct timespec before, after, total;
  total.tv_sec = 0;
  total.tv_nsec = 0;
  
  if (newArgs->syncOpt == 'm') {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      clock_gettime(CLOCK_REALTIME, &before);
      pthread_mutex_lock(&mutex[index]);
      clock_gettime(CLOCK_REALTIME, &after);
      addTimeDif(&before, &after, &total);
      
      SortedList_insert(&(newArgs->heads[index]), &(newArgs->elementArr[i]));
      pthread_mutex_unlock(&mutex[index]);
    }

    // get list length - synchronization is handled inside getTotalLength 
    if (-1 == getTotalLength(newArgs->heads, newArgs->listNum, newArgs->syncOpt, &total.tv_nsec)) {
      corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
    }
      
    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      //Lookup element
      clock_gettime(CLOCK_REALTIME, &before);
      pthread_mutex_lock(&mutex[index]);
      clock_gettime(CLOCK_REALTIME, &after);
      addTimeDif(&before, &after, &total);
      
      SortedListElement_t *target = SortedList_lookup(&(newArgs->heads[index]), newArgs->elementArr[i].key);
      if (target == NULL) {
	corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
      }

      //Delete target
      if (1 == SortedList_delete(target)) {
	corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
      }
      pthread_mutex_unlock(&mutex[index]);
    }
  }

  else if (newArgs->syncOpt == 's') {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      clock_gettime(CLOCK_REALTIME, &before);
      while (__sync_lock_test_and_set(&lock[index], 1));
      clock_gettime(CLOCK_REALTIME, &after);
      addTimeDif(&before, &after, &total);
      
      SortedList_insert(&(newArgs->heads[index]), &(newArgs->elementArr[i]));

      __sync_lock_release(&lock[index]);
    }

    // get list length - synchronization handled in getTotalLength()
    if (-1 == getTotalLength(newArgs->heads, newArgs->listNum, newArgs->syncOpt, &total.tv_nsec)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);

    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      //Lookup element
      clock_gettime(CLOCK_REALTIME, &before);
      while (__sync_lock_test_and_set(&lock[index], 1));
      clock_gettime(CLOCK_REALTIME, &after);
      addTimeDif(&before, &after, &total);
      
      SortedListElement_t *target = SortedList_lookup(&(newArgs->heads[index]), newArgs->elementArr[i].key);
      if (target == NULL) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);

      //Delete target
      if (1 == SortedList_delete(target)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
      __sync_lock_release(&lock[index]);
    }
  }

  else {
    //Insert Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      SortedList_insert(&(newArgs->heads[index]), &(newArgs->elementArr[i]));
    }

    // get list length
    if (-1 == getTotalLength(newArgs->heads, newArgs->listNum, newArgs->syncOpt, NULL)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
    
    // Delete Elements
    for (i = 0; i < newArgs->arrLen; i++) {
      int index = listIndex(newArgs->elementArr[i].key, newArgs->listNum);
      
      //Lookup element
      SortedListElement_t *target = SortedList_lookup(&(newArgs->heads[index]), newArgs->elementArr[i].key);
      if (target == NULL) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
      
      //Delete target
      if (1 == SortedList_delete(target)) corruptedList(newArgs->elementArrStart, newArgs->threadIDs, newArgs->keyArrStart, newArgs->args, newArgs->waitTimeStart, newArgs->mutexStart, newArgs->lockStart, newArgs->heads);
    }
  }

  *(newArgs->waitTimePtr) = total.tv_nsec + 1e9 * total.tv_sec;
  
  return NULL;
}

int main(int argc, char **argv) {

  //Adjustable Constants
  int keyLen = 20;//Number of chars reserved for each key string (including null-terminator)

  //Seed random number generator
  srand(time(NULL));
  
  //Option parsing
  int threadNum, iterNum, listNum;
  char syncOpt;
  optionParser(argc, argv, &threadNum, &iterNum, &syncOpt, &listNum);

  //Initialize locks/mutex
  if (pthread_mutex_init(&errorMutex, NULL)) {
    fprintf(stderr, "Error initializing mutex.\n");
    exit(1);
  }
  
  mutex = malloc(listNum * sizeof(pthread_mutex_t));
  lock = malloc(listNum * sizeof(int));

  int i;
  for (i = 0; i < listNum; i++) {
    lock[i] = 0;
    if (pthread_mutex_init(&mutex[i], NULL)) {
      fprintf(stderr, "Error initializing mutex.\n");
      free(mutex);
      free((int*) lock);
      exit(1);
    }
  }

  //Create shared head(s) of list
  SortedList_t *head = malloc(listNum * sizeof(SortedList_t));
  for (i = 0; i < listNum; i++) {
    head[i].next = &head[i];
    head[i].prev = &head[i];
    head[i].key = NULL;
  }

  //Set up segfault handler
  signal(SIGSEGV, segHandler);
  
  //Allocate mem for elements, thread IDs, keys, and arg structures
  SortedListElement_t *elements = malloc(threadNum * iterNum * sizeof(SortedListElement_t));
  pthread_t *threads = malloc(threadNum * sizeof(pthread_t));
  char *keys = malloc(threadNum * iterNum * keyLen * sizeof(char));
  for (i = 0; i < threadNum * iterNum; i++) {
    int j;
    for (j = 0; j < keyLen - 1; j++)
      keys[i * keyLen + j] = rand() % (CHAR_MAX + 1);
    keys[i * keyLen + keyLen - 1] = '\0';
    
    elements[i].key = &keys[i];
  }

  long *waitTimes = malloc(threadNum * sizeof(long));
  
  struct threadArgs *args = malloc(threadNum * sizeof(struct threadArgs));
  for (i = 0; i < threadNum; i++) {
    args[i].heads = head;
    args[i].elementArr = &elements[i * iterNum];
    args[i].arrLen = iterNum;
    args[i].syncOpt = syncOpt;
    args[i].waitTimePtr = &waitTimes[i];
    args[i].listNum = listNum;
    args[i].elementArrStart = elements;
    args[i].threadIDs = threads;
    args[i].keyArrStart = keys;
    args[i].args = args;
    args[i].waitTimeStart = waitTimes;
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
      free(waitTimes);
      free(mutex);
      free((int*) lock);
      free(head);
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
      free(waitTimes);
      free(mutex);
      free((int*) lock);
      free(head);
      exit(1);
    }
  }
  

  //Get time
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);
  
  
  //Check if length is 0 - can ignore synchronization, since there is only 1 thread at this point
  if (0 != getTotalLength(head, listNum, ' ', NULL)) {
    pthread_mutex_lock(&errorMutex);
    
    fprintf(stderr, "Non-zero length of final list!\n");
    free(elements);
    free(threads);
    free(keys);
    free(args);
    free(waitTimes);
    free(mutex);
    free((int*) lock);
    free(head);
    exit(1);
  }

  //Find total waiting time
  long totalWaitTime = 0;
  for (i = 0; i < threadNum; i++) {
    totalWaitTime += waitTimes[i];
  }
  int aveWaitTime = totalWaitTime / (threadNum * (2 * iterNum + 1));

  //Free memory (thread IDs and elements)
  free(elements);
  free(threads);
  free(keys);
  free(args);
  free(waitTimes);
  free(mutex);
  free((int*) lock);
  free(head);
  

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

  printf("list-%s%s%s-%s,%i,%i,%i,%i,%ld,%i,%i\n", yieldopts1, yieldopts2, yieldopts3, syncopts, threadNum, iterNum, listNum, opNum, nsecDif, aveTime, aveWaitTime);

  exit(0);
}

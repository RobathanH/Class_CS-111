//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0


#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>


//Globals
int opt_yield;
char opt_sync;
int lock;
pthread_mutex_t mutex;

void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab2_add [--threads=#] [--iterations=#] [--yield] [--sync=[m,s,c]]\n");
  exit(1);
}

void formatErrBadNum() {
  fprintf(stderr, "Incorrect Format: argument values must be nonzero positive integers.\n");
  exit(1);
}

void formatErrBadChar() {
  fprintf(stderr, "Incorrect Format: Argument for --sync option must be one of [m, s, c].\n");
  exit(1);
}

void optionParser(int argc, char **argv, int *threadNumPtr, int *iterNumPtr) {
  //Unset Values - so function can tell whether the option has been set yet
  *threadNumPtr = -1;
  *iterNumPtr = -1;
  opt_yield = 0;//Boolean value
  opt_sync = 0;
  
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
  longopts[2].has_arg = 0;
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

      opt_yield = 1;
      break;

    case 's':
      if (opt_sync != 0) formatErr();

      if (strlen(optarg) != 1 || (optarg[0] != 'm' && optarg[0] != 's' && optarg[0] != 'c')) formatErrBadChar();
      
      opt_sync = optarg[0];
      break;
      
    default:
      formatErr();
    }
  }

  //Apply default values if arguments haven't been given
  if (*threadNumPtr == -1) *threadNumPtr = 1;
  if (*iterNumPtr == -1) *iterNumPtr = 1;
}


//Thread add routine
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;

  if (opt_yield) sched_yield();

  *pointer = sum;
}

//thread add routine with mutex
void addM(long long *pointer, long long value) {

  //Lock mutex
  pthread_mutex_lock(&mutex);

  long long sum = *pointer + value;

  if (opt_yield) sched_yield();

  *pointer = sum;

  //release mutex
  pthread_mutex_unlock(&mutex);
}

//thread add routine with spinlock
void addS(long long *pointer, long long value) {
  //Busy wait for lock to be free
  while (__sync_lock_test_and_set(&lock, 1) == 1);

  long long sum = *pointer + value;

  if (opt_yield) sched_yield();

  *pointer = sum;

  //release lock
  __sync_lock_release(&lock);
}

//thread add routine with compare_and_swap
void addC(long long *pointer, long long value) {
  long long sum, oldval; 
  
  do {
    oldval = *pointer;
    sum = oldval + value;
    
    if (opt_yield) sched_yield();

  } while (oldval != __sync_val_compare_and_swap(pointer, oldval, sum));
}

//thread handler arg struct
struct threadArgs {
  long long *pointer;
  int iter;
};

//thread handler routine
void *threadHandler(void *arg) {
  struct threadArgs *argstruct = (struct threadArgs *) arg;
  int iter = argstruct->iter;
  long long *pointer = argstruct->pointer;
  
  int i;
  switch(opt_sync) {
  case 'm':
    for (i = 0; i < iter; i++) addM(pointer, 1);
    for (i = 0; i < iter; i++) addM(pointer, -1);
    break;

  case 's':
    for (i = 0; i < iter; i++) addS(pointer, 1);
    for (i = 0; i < iter; i++) addS(pointer, -1);
    break;

  case 'c':
    for (i = 0; i < iter; i++) addC(pointer, 1);
    for (i = 0; i < iter; i++) addC(pointer, -1);
    break;

  default:
    for (i = 0; i < iter; i++) add(pointer, 1);
    for (i = 0; i < iter; i++) add(pointer, -1);
  }
  
  return NULL;
}

int main(int argc, char **argv) {
  
  //Option parsing
  int threadNum, iterNum;
  optionParser(argc, argv, &threadNum, &iterNum);
  
  //Get starting time
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);

  //Declare shared variable
  long long sum = 0;

  //Initialize locks/mutex
  lock = 0;
  if (0 != pthread_mutex_init(&mutex, NULL)) {
    fprintf(stderr, "Error initializing mutex.\n");
    exit(1);
  }
  
  //Thread ID array
  pthread_t *threads = malloc(threadNum * sizeof(pthread_t));

  //threadhandler arg structure
  struct threadArgs args;
  args.pointer = &sum;
  args.iter = iterNum;
  
  //Run threads
  int i, val;
  for (i = 0; i < threadNum; i++) {
    if (0 != (val = pthread_create(&threads[i], NULL, &threadHandler, (void *) &args))) {
      fprintf(stderr, "Error creating threads: ");
      switch(val) {
      case EAGAIN:
	fprintf(stderr, "System lacks necessary resources or too many threads.\n");
	break;
      case EINVAL:
	fprintf(stderr, "Invalid attr argument.\n");
	break;
      case EPERM:
	fprintf(stderr, "Inappropriate permissions to set scheduling parameters or policy.\n");
      }

      free(threads);
      exit(1);
    }
  }

  //Wait for threads to finish
  for (i = 0; i < threadNum; i++) {
    if (0 != (val = pthread_join(threads[i], NULL))) {
      fprintf(stderr, "Error joining threads: ");
      switch(val) {
      case EINVAL:
	fprintf(stderr, "Thread ID is invalid.\n");
	break;
      case ESRCH:
	fprintf(stderr, "No thread found with given ID.\n");
	break;
      case EDEADLK:
	fprintf(stderr, "Deadlock detected.\n");
      }

      free(threads);
      exit(1);
    }
  }

  //Record ending time
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);
  
  //Find elapsed time
  time_t secDif = end.tv_sec - start.tv_sec;
  long nsecDif = end.tv_nsec - start.tv_nsec;

  nsecDif += secDif * 1e9;
  
  //Print end result
  char *NAME;
  if (opt_yield) {
    switch(opt_sync) {
    case 'm':
      NAME = "add-yield-m";
      break;

    case 's':
      NAME = "add-yield-s";
      break;

    case 'c':
      NAME = "add-yield-c";
      break;

    default:
      NAME = "add-yield-none";
    }
  }
  else {
    switch (opt_sync) {
    case 'm':
      NAME = "add-m";
      break;

    case 's':
      NAME = "add-s";
      break;

    case 'c':
      NAME = "add-c";
      break;

    default:
      NAME = "add-none";
    }
  }
  
  int opNum = threadNum * iterNum * 2;
  int aveTime = nsecDif / opNum;
  printf("%s,%i,%i,%i,%ld,%i,%lld\n", NAME, threadNum, iterNum, opNum, nsecDif, aveTime, sum);


  //Exit
  free(threads);
  exit(0);
}









    

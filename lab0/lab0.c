//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>


//Error function
void formatError() {
  fprintf(stderr, "Incorrect Command Format: ./lab0 [--input=fileName] [--output=fileName] [--segfault] [--catch]\n");
  exit(1);
}

//segfault handler
void segHandler() {
  fprintf(stderr, "Segmentation fault correctly handled.\n");
  exit(4);
}

//segfault function
void causeSegFault() {
  char *stupidVar = NULL;
  *stupidVar = 0;
}

int main(int argc, char *argv[]) {

  //CONSTANT
  int BUFSIZE = 100; //buffer size in chars
  
  //SETUP for getopt_long()
  char *optstring = "";
  int *longindex = NULL;
  
  struct option longopts[4];
  longopts[0].name = "input";
  longopts[0].has_arg = 1;
  longopts[0].flag = NULL;
  longopts[0].val = 'i';
  longopts[1].name = "output";
  longopts[1].has_arg = 1;
  longopts[1].flag = NULL;
  longopts[1].val = 'o';
  longopts[2].name = "segfault";
  longopts[2].has_arg = 0;
  longopts[2].flag = NULL;
  longopts[2].val = 's';
  longopts[3].name = "catch";
  longopts[3].has_arg = 0;
  longopts[3].flag = NULL;
  longopts[3].val = 'c';
  
  opterr = 0;

  //Default variable values
  int fdIn = 0;
  int fdOut = 1;
  int signalHandler = 0;
  int segFault = 0;
  int inputChange = 0;
  int outputChange = 0;
  
  //getopt use
  int val;
  while (-1 != (val = getopt_long(argc, argv, optstring, longopts, longindex))) {
    switch(val) {
    case 'i':
      if (inputChange)
	formatError();

      inputChange = 1;
      int tempfd = open(optarg, O_RDONLY);
      fdIn = dup2(tempfd, fdIn);
      close(tempfd);

      //error check
      if (fdIn < 0) {
	fprintf(stderr, "Error in input file: %s\n", strerror(errno));
	exit(2);
      }
      break;

    case 'o':
      if (outputChange)
	formatError();

      inputChange = 1;
      int tempfd2 = open(optarg, O_WRONLY | O_CREAT, 0666);
      fdOut = dup2(tempfd2, fdOut);
      close(tempfd2);

      //error check
      if (fdOut < 0) {
	fprintf(stderr, "Error in output file: %s\n", strerror(errno));
	exit(3);
      }
      break;
      
    case 's':
      segFault = 1;
      break;

    case 'c':
      signalHandler = 1;
      //segFault = 1;     //SHOULD --catch AUTOMATICALLY CAUSE A SEGFAULT? ASSUMING NO
      break;

    case '?':
    case ':':
      formatError();
    }
  }

  //Actual Operations
  
  if (signalHandler)
    signal(SIGSEGV, &segHandler);

  if (segFault)
    causeSegFault();

  char *buffer = malloc(BUFSIZE * sizeof(char));
  int charNum;
  if (buffer == NULL)
    exit(-1);
  while (0 != (charNum = read(fdIn, buffer, BUFSIZE)))
    write(fdOut, buffer, charNum);

  free(buffer);
  close(fdIn);
  close(fdOut);
  exit(0);
}

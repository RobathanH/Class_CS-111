//NAME: Robathan Harries
//ID: 904836501
//EMAIL: r0b@ucla.edu
//SLIPDAYS: 0

#include <stdio.h>
#include <poll.h>
#include <getopt.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <mraa.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#define BUFFER_SIZE 100

#define DEF_PERIOD 1
#define DEF_SCALE 'F'

#define MRAA_BUTTON_PIN 60
#define MRAA_TEMPSENSOR_PIN 1

//The program will loop until this is set to 0 by buttonPressed()
volatile int run = 1;

//Allows interrupt routines to print to logfile - set to NULL by default
FILE *logfd = NULL;
//Allows interrupt routines to know how to format temperature
char tempType;
//Allows interrupt routines to read from sensor
mraa_aio_context sensor;
//Allows interrupt routines to write through ssl
SSL *ssl;


//Print error message and exit
void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab4b portnumber --log=filename --id=IDnumber --host=hostname [--period=#] [--scale=[C,F]]\n");
  exit(1);
}

//Error for incorrectly formatted number
void badNumErr() {
  fprintf(stderr, "Incorrect Number Format: --period argument must be a positive number.\n");
  exit(1);
}

void badCharErr() {
  fprintf(stderr, "Incorrect Argument Format: --scale argument must be 1 character: either C or F.\n");
  exit(1);
}

void badIDErr() {
  fprintf(stderr, "Incorrect Argument Format: --id argument must be 9 digits.\n");
  exit(1);
}

void mandatoryArgErr() {
  fprintf(stderr, "Missing Mandatory Options: --id, --host, --log, and portnumber arguments are all mandatory.\n");
  exit(1);
}

void badPortErr() {
  fprintf(stderr, "Incorrect Argument Format: Port argument must be positive integer.\n");
  exit(1);
}

//Given the command line variables and pointers to a period var (double), scale var (char), and logfile name (char pointer), this parses through command line options
void optionParser(int argc, char **argv, double *perPtr, char *scalePtr, char **logPtr, char **idPtr, char **hostPtr, int *portPtr) {
  char *optstring = "";
  int *longindex = NULL;

  struct option longopts[6];
  longopts[0].name = "period";
  longopts[0].has_arg = 1;
  longopts[0].flag = NULL;
  longopts[0].val = 'p';
  longopts[1].name = "scale";
  longopts[1].has_arg = 1;
  longopts[1].flag = NULL;
  longopts[1].val = 's';
  longopts[2].name = "log";
  longopts[2].has_arg = 1;
  longopts[2].flag = NULL;
  longopts[2].val = 'l';
  longopts[3].name = "id";
  longopts[3].has_arg = 1;
  longopts[3].flag = NULL;
  longopts[3].val = 'i';
  longopts[4].name = "host";
  longopts[4].has_arg = 1;
  longopts[4].flag = NULL;
  longopts[4].val = 'h';
  longopts[5].name = NULL; //NULL-termination entry
  longopts[5].has_arg = 0;
  longopts[5].flag = NULL;
  longopts[5].val = 0;

  opterr = 0;

  //Set values to impossible values
  *perPtr = 0;
  *scalePtr = 0;
  *logPtr = NULL;
  *idPtr = NULL;
  *hostPtr = NULL;

  int val;
  while (-1 != (val = getopt_long(argc, argv, optstring, longopts, longindex))) {
    switch(val) {
    case 'p':
      //Check for repeat options
      if (*perPtr) formatErr();

      double num = atof(optarg);
      if (num == 0) badNumErr(); // works whether num == 0 indicates not number or just number = 0
      *perPtr = num;

      break;

    case 's':
      //Check for repeats
      if (*scalePtr) formatErr();

      if (strlen(optarg) != 1 || (optarg[0] != 'C' && optarg[0] != 'F')) badCharErr();

      *scalePtr = *optarg;

      break;

    case 'l':
      //Check for repeats
      if (*logPtr) formatErr();

      if (strlen(optarg) == 0) formatErr(); //no empty filenames allowed
      
      //allocate space
      *logPtr = malloc((strlen(optarg) + 1) * sizeof(char));
      strcpy(*logPtr, optarg);

      break;

    case 'i':
      //Check for repeats
      if (*idPtr) formatErr();

      if (strlen(optarg) != 9) badIDErr();

      //check that it is number
      int numInt = atoi(optarg);
      if (numInt <= 0) badIDErr();
      
      //allocate space
      *idPtr = malloc(10 * sizeof(char));
      strcpy(*idPtr, optarg);

      break;

    case 'h':
      //Check for repeats
      if (*hostPtr) formatErr();

      if (strlen(optarg) == 0) formatErr();
      
      //allocate space
      *hostPtr = malloc((strlen(optarg) + 1) * sizeof(char));
      strcpy(*hostPtr, optarg);

      break;
				 
    default:
      formatErr();
    }
  }

  // Get port number
  int i;
  *portPtr = 0;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      // Check for repeats
      if (*portPtr)
	formatErr();

      int num = atoi(argv[i]);
      if (num == 0)
	badPortErr();
      *portPtr = num;
    }
  }

  //Change any unaltered variables to default values
  if (!*perPtr) *perPtr = DEF_PERIOD;
  if (!*scalePtr) *scalePtr = DEF_SCALE;

  // Check mandatory arguments
  if (!*logPtr || !*idPtr || !*hostPtr || !*portPtr) mandatoryArgErr();
}

// logs message without sending it across the socket - good for input
void logMessage(char *message) {
  fprintf(logfd, "%s\n", message);
}

//Prints the time followed by the given message
void printTime(char *message) {
  time_t rawtime;
  time(&rawtime);
    
  struct tm *info = localtime(&rawtime);

  fprintf(logfd, "%02i:%02i:%02i %s\n", info->tm_hour, info->tm_min, info->tm_sec, message);

  char *tempBuf = malloc(11 + strlen(message));

  snprintf(tempBuf, 11 + strlen(message), "%02i:%02i:%02i %s\n", info->tm_hour, info->tm_min, info->tm_sec, message);
  SSL_write(ssl, tempBuf, strlen(tempBuf));
  
  free(tempBuf);
}

//This function is called when 
void stopRunning() {
  run = 0;

  printTime("SHUTDOWN");
}

//This function gets called on sigalrm signal, which is scheduled by timer
void tempSensor(int signal) {
  if (signal == SIGALRM) {
    uint16_t value = mraa_aio_read(sensor);
    double temp = 1.0 / (log10(1023.0 / value - 1.0) / 4275 + 1 / 298.15) - 273.15;

    if (tempType == 'F') temp = temp * 1.8 + 32;

    char tempString[6];
    snprintf(tempString, 6, "%.1f", temp);
    
    printTime(tempString);
  }
}

//tests whether the first part of string a matches the entirety of string 'template'
//Returns 1 if it does, 0 if it doesn't (or if len(a) < len(template))
//Both a and template must be null-terminated
int stringChecker(char *a, char *template) {
  int i = 0;
  while (a[i] != '\0' && template[i] != '\0' && a[i] == template[i]) i++;

  if (template[i] == '\0') return 1;
  return 0;
}

//Fills a itimerval structure with a given period in seconds
void fillTimerStruct(double period, struct itimerval *timer) {
  timer->it_interval.tv_sec = timer->it_value.tv_sec = (int) period;
  timer->it_interval.tv_usec = timer->it_value.tv_usec = ((int) (period * 1e6)) % (int) 1e6;
}


int main(int argc, char **argv) {

  //Option variables
  double period;
  char *log;
  char *id;
  char *hostName;
  int portNum;
  optionParser(argc, argv, &period, &tempType, &log, &id, &hostName, &portNum);


  //Open log file
  if (log != NULL && NULL == (logfd = fopen(log, "a"))) {
    fprintf(stderr, "Error opening file %s: %s\n", log, strerror(errno));
    free(log);
    free(id);
    free(hostName);
    exit(1);
  }

  //Setup TCP socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    fprintf(stderr, "Error opening socket: %s\n", strerror(errno));
    free(log);
    free(id);
    free(hostName);
    exit(1);
  }
  struct hostent *server;
  struct sockaddr_in addr;
  server = gethostbyname(hostName);
  if (server == NULL) {
    fprintf(stderr, "Error using host name: %s\n", strerror(errno));
    free(log);
    free(id);
    free(hostName);
    exit(1);
  }
  memset((char*) &addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  memcpy((char*) &addr.sin_addr.s_addr, (char*) server->h_addr, server->h_length);
  addr.sin_port = htons(portNum);
  if (connect(sockfd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
    fprintf(stderr, "Error connecting socket to server address: %s\n", strerror(errno));
    free(log);
    free(id);
    free(hostName);
    exit(2);
  }

  //Setup ssl
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  if (SSL_library_init() < 0) {
    fprintf(stderr, "Error loading OpenSSL.\n");
    free(log);
    free(id);
    free(hostName);
    exit(2);
  }

  SSL_CTX *context = SSL_CTX_new(TLSv1_client_method());
  if (context == NULL) {
    fprintf(stderr, "Error creating SSL context.\n");
    free(log);
    free(id);
    free(hostName);
    exit(2);
  }

  ssl = SSL_new(context);
  if (!SSL_set_fd(ssl, sockfd) || SSL_connect(ssl) != 1) {
    fprintf(stderr, "Error connecting SSL: %s\n", strerror(errno));
    free(log);
    free(id);
    free(hostName);
    exit(2);
  }
  
  //Setup MRAA context variable
  sensor = mraa_aio_init(MRAA_TEMPSENSOR_PIN);

  //Begin by sending ID message across socket
  char message[13];
  snprintf(message, 14, "ID=%s\n", id);
  fprintf(logfd, "%s", message);
  SSL_write(ssl, message, strlen(message));
  
  //Set recurring alarm
  struct itimerval periodStruct;
  fillTimerStruct(period, &periodStruct);
  
  signal(SIGALRM, tempSensor);
  setitimer(ITIMER_REAL, &periodStruct, NULL);

  //Force one report before taking inputs
  tempSensor(SIGALRM);

  //Create polling structure
  struct pollfd poller;
  poller.fd = sockfd;
  poller.events = POLLIN;
  
  //Create buffer
  char buffer[BUFFER_SIZE]; //stores input from multiple reads until newline is read
  char tempBuffer[BUFFER_SIZE]; //stores input from a particular read
  
  int i = 0; //position of next free element in buffer - also equals length
  
  while (run) {
    int val = poll(&poller, 1, 0);
    if (val == -1 && errno != EINTR) {
      fprintf(stderr, "Error polling: %s\n", strerror(errno));
      mraa_aio_close(sensor);
      free(log);
      free(id);
      free(hostName);
      exit(1);
    }
    else if (val == 1) {
      int len = SSL_read(ssl, tempBuffer, BUFFER_SIZE);
      
      if (len <= 0) {
        fprintf(stderr, "Error reading from socket!\n");
      	mraa_aio_close(sensor);
	free(log);
	free(id);
	free(hostName);
	exit(1);
      }

      int j;
      for (j = 0; j < len && i < BUFFER_SIZE; j++) {
	if (tempBuffer[j] == '\n') {
	  buffer[i] = '\0';

	  //Read buffer for input options - write them to log file

	  logMessage(buffer);

	  //SCALE
	  if (stringChecker(buffer, "SCALE=") && strlen(buffer) == 7 && (buffer[6] == 'F' || buffer[6] == 'C')) {
	    	tempType = buffer[6];
	  }

	  //PERIOD
	  else if (stringChecker(buffer, "PERIOD=")) {
	    double num;
	    if (0 != (num = atof(&buffer[7]))) {
	      period = num;
	      fillTimerStruct(period, &periodStruct);
              setitimer(ITIMER_REAL, &periodStruct, NULL);
	    }
	  }

	  //STOP
	  else if (stringChecker(buffer, "STOP") && strlen(buffer) == 4) {
	    fillTimerStruct(0, &periodStruct);
	    setitimer(ITIMER_REAL, &periodStruct, NULL);
	  }

	  //START
	  else if (stringChecker(buffer, "START") && strlen(buffer) == 5) {
	    fillTimerStruct(period, &periodStruct);
	    setitimer(ITIMER_REAL, &periodStruct, NULL);
	  }

	  //LOG
	  else if (stringChecker(buffer, "LOG ")) {
	    //Currently does nothing
	  }

	  //OFF
	  else if (stringChecker(buffer, "OFF")) {
	    stopRunning();
	  }

	  //Unrecognized command - currently, do nothing
	  else {}
	  
	  i = 0;
	}
	else {
	  buffer[i++] = tempBuffer[j];
	}
      }
      //Error if input is too large without a newline
      if (i >= BUFFER_SIZE) {
	fprintf(stderr, "Too much input without newline to clear buffer.\n");
	mraa_aio_close(sensor);
	free(log);
	free(id);
	free(hostName);
	exit(1);
      }
    }
  }
  
  mraa_aio_close(sensor);
  free(log);
  exit(0);
}

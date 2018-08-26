//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <wait.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ulimit.h>
#include "zlib.h"
#include <assert.h>

//Global terminal variables - allows functions outside of main to exit correctly
struct termios *old;
struct termios *new;

void setNewTermAttr() {
  old = malloc(sizeof(struct termios));
  new = malloc(sizeof(struct termios));
  
  tcgetattr(0, old);
  *new = *old;
  new->c_iflag = ISTRIP;
  new->c_oflag = 0;
  new->c_lflag = 0;

  tcsetattr(0, TCSANOW, new);
}

void revertTermAttr() {
  tcsetattr(0, TCSANOW, old);
  free(old);
  free(new);
}

void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab1b-client --port=# [--log=filename] [--compress]\n");
  //TERMINAL SHOULD NOT HAVE BEEN CHANGED AT THIS POINT, so now terminal reversion necessary
  exit(1);
}

void optionParser(int argc, char **argv, int *portPtr, char **fileNamePtr, int *compressPtr) {
  //Argument Checker
  char *optstring = "";
  int *longindex = NULL;

  struct option longopts[4];
  longopts[0].name = "port";
  longopts[0].has_arg = 1;
  longopts[0].flag = NULL;
  longopts[0].val = 'p';
  longopts[1].name = "log";
  longopts[1].has_arg = 1;
  longopts[1].flag = NULL;
  longopts[1].val = 'l';
  longopts[2].name = "compress";
  longopts[2].has_arg = 0;
  longopts[2].flag = NULL;
  longopts[2].val = 'c';
  longopts[3].name = NULL; //NULL-termination entry
  longopts[3].has_arg = 0;
  longopts[3].flag = NULL;
  longopts[3].val = 0;

  opterr = 0;

  //Default vals
  *portPtr = -1;
  *fileNamePtr = NULL;
  *compressPtr = 0;

  int val = getopt_long(argc, argv, optstring, longopts, longindex);
  while (val != -1) {
    switch(val) {
    case 'p':
      if (*portPtr != -1) formatErr(); //Since port has already been set once

      //Test that port num is valid
      int i;
      if (optarg != 0) {
	for (i = 0; optarg[i] != 0; i++)
	  if (!isdigit(optarg[i])) formatErr();
	*portPtr = atoi(optarg);
      }
      break;

    case 'l':
      if (*fileNamePtr != NULL) formatErr();
      *fileNamePtr = optarg;
      break;

    case 'c':
      if (*compressPtr != 0) formatErr();
      *compressPtr = 1;
      break;
      
    default:
      formatErr();
    }
    val = getopt_long(argc, argv, optstring, longopts, longindex);
  }

  //Check that mandatory --port arg has been given
  if (*portPtr == -1) formatErr();
}


int setupSocket(int port) {
  
  //Constants
  int domain = PF_INET;
  int domainForAddr = AF_INET;
  int type = SOCK_STREAM;
  int proto = 0;
  int sockfd;
  char *localhost = "127.0.0.1";
  struct sockaddr_in address;
  bzero((char *) &address, sizeof(address));
  address.sin_family = domainForAddr;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = inet_addr(localhost);

  if ((sockfd = socket(domain, type, proto)) < 0) {
    fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
    revertTermAttr();
    close(sockfd);
    exit(1);
  }
  
  if (connect(sockfd, (struct sockaddr *) &address, sizeof(address)) < 0) {
    fprintf(stderr, "Error connecting to server: %s\n", strerror(errno));
    revertTermAttr();
    close(sockfd);
    exit(1);
  }

  return sockfd;
}


int main(int argc, char **argv) {
  
  int port;
  char *fileName;
  int compress;
  optionParser(argc, argv, &port, &fileName, &compress);

  //File stuff for --log

  if (ulimit(UL_SETFSIZE, 10000) < 0) {
    fprintf(stderr, "Error limiting file size: %s\n", strerror(errno));
    exit(1);
  }
  int fd;
  if (fileName != NULL) {
    fd = creat(fileName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd < 0) {
      fprintf(stderr, "Error opening file %s: %s\n", fileName, strerror(errno));
      exit(1);
    }
  }
  
  setNewTermAttr();

  //Set up socket
  int sockfd = setupSocket(port);

  //Polling structure
  struct pollfd inputs[2];
  inputs[0].fd = 0;
  inputs[0].events = POLLIN;
  inputs[1].fd = sockfd;
  inputs[1].events = POLLIN;


  //Set up compression variables
  z_stream defstream, infstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  deflateInit(&defstream, Z_BEST_COMPRESSION);
  inflateInit(&infstream);

  int FLUSH_MODE = Z_SYNC_FLUSH;
  
  //Reading
  int BUFSIZE = 245;
  int TIMEOUT = 1000;
  char *buffer = malloc(BUFSIZE* sizeof(char));

  int charsRead;
  int origCharsRead = 1;
  int end = 0;

  while(!end && origCharsRead > 0) {
    int ret;
    while ((ret = poll(inputs, 2, TIMEOUT)) == 0);

    if (ret < 0) {//Error
      fprintf(stderr, "Error polling input streams: %s\n", strerror(errno));
      revertTermAttr();
      free(buffer);
      close(sockfd);
      deflateEnd(&defstream);
      inflateEnd(&infstream);
      exit(1);
    }

    if ((inputs[0].revents | inputs[1].revents) & POLLERR) {
      fprintf(stderr, "Error in polling.\n");
      revertTermAttr();
      free(buffer);
      close(sockfd);
      deflateEnd(&defstream);
      inflateEnd(&infstream);
      exit(1);
    }

    if (inputs[1].revents & POLLIN) {
      if ((origCharsRead = read(sockfd, buffer, BUFSIZE)) > 0) {

	charsRead = origCharsRead;
	
	//Log
	if (fileName != NULL) {
	  char *buffer2 = malloc(BUFSIZE * sizeof(char));
	  int i;
	  for (i = 0; i < charsRead; i++)
	    buffer2[i] = buffer[i];

	  buffer2[charsRead] = '\0';
	  dprintf(fd, "RECEIVED %i bytes: %s\n", charsRead, buffer2);
	  free(buffer2);
	}

	//Compression
	if (compress) {
	  char *tempBuffer = malloc(BUFSIZE * sizeof(char));
	  infstream.avail_in = charsRead;
	  infstream.next_in = (Bytef*) buffer;
	  infstream.avail_out = BUFSIZE;
	  infstream.next_out = (Bytef*) tempBuffer;
	  
	  inflate(&infstream, FLUSH_MODE);

	  if (infstream.avail_in > 0) {
	    int size = infstream.next_out - (Bytef*) tempBuffer;
	    tempBuffer = realloc(tempBuffer, 2 * BUFSIZE);
	    infstream.avail_out = BUFSIZE;
	    infstream.next_out = (Bytef*) (tempBuffer + size);
	    inflate(&infstream, FLUSH_MODE);
	  }
	  
	  //inflateReset(&infstream);
	  free(buffer);
	  buffer = tempBuffer;
	  charsRead = (char *) infstream.next_out - buffer;

        }

	int i;
	for (i = 0; i < charsRead && !end; i++) {
	  char temp = buffer[i];
	  switch(temp) {
	  case 10:
	  case 13:
	    temp = 13;
	    char temp2 = 10;
	    
	    if (write(1, &temp, 1) < 1 || write(1, &temp2, 1) < 1) {
	      fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
	      free(buffer);
	      revertTermAttr();
	      close(sockfd);
	      deflateEnd(&defstream);
	      inflateEnd(&infstream);
	      exit(1);
	    }
	    break;

	  case 4:
	    end = 1;
	    break;

	  default:
	    if (write(1, &temp, 1) < 1) {
	      fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
	      free(buffer);
	      revertTermAttr();
	      close(sockfd);
	      deflateEnd(&defstream);
	      inflateEnd(&infstream);
	      exit(1);
	    }
	  }
	}
      }
    }
    
    else if (inputs[0].revents & POLLIN) {//Input from keyboard
      if ((origCharsRead = read(0, buffer, BUFSIZE)) > 0) {

	charsRead = origCharsRead;

	//Echo to keyboard and apply transformations
	int i;
	for (i = 0; i < charsRead && !end; i++) {
	  char temp = buffer[i];
	  switch(temp) {
	  case 10:
	  case 13:
	    temp = 13;
	    char temp2 = 10;
	    if (write(1, &temp, 1) < 1 || write(1, &temp2, 1) < 1) {
	      fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
	      free(buffer);
	      revertTermAttr();
	      close(sockfd);
	      deflateEnd(&defstream);
	      inflateEnd(&infstream);
	      exit(1);
	    }

	    //Transform either into LineFeed
	    buffer[i] = 10; //10 = linefeed
	    break;

	  case 4:
	  case 3:
	    break;
	    
	  default:
	    if (write(1, &temp, 1) < 1) {
	      fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
	      free(buffer);
	      revertTermAttr();
	      close(sockfd);
	      deflateEnd(&defstream);
	      inflateEnd(&infstream);
	      exit(1);
	    }
	  }
	}

	//Compression
	if (compress) {
	  char *tempBuffer = malloc(BUFSIZE * sizeof(char));
	  defstream.avail_in = charsRead;
	  defstream.next_in = (Bytef*) buffer;
	  defstream.avail_out = BUFSIZE;
	  defstream.next_out = (Bytef*) tempBuffer;

	  deflate(&defstream, FLUSH_MODE);
	  if (defstream.avail_in > 0) {
	    int size = defstream.next_out - (Bytef*) tempBuffer;
	    tempBuffer = realloc(tempBuffer, 2 * BUFSIZE);
	    defstream.avail_out = BUFSIZE;
	    defstream.next_out = (Bytef*) (tempBuffer + size);
	    deflate(&defstream, FLUSH_MODE);
	  }

	  //deflateReset(&defstream);
	  free(buffer);
	  buffer = tempBuffer;
	  charsRead = (char *) defstream.next_out - buffer;
	}

	//Set up for logging
	int charsSent;
	char *buffer2;
	if (fileName != NULL) {
	  charsSent = 0;
	  buffer2 = malloc(2 * BUFSIZE * sizeof(char));
	}

	//write to socket
	for (i = 0; i < charsRead && !end; i++) {
	  char temp = buffer[i];
	  if (write(sockfd, &temp, 1) < 1) {
	    fprintf(stderr, "Error writing to stdout and server: %s\n", strerror(errno));
	    free(buffer);
	    revertTermAttr();
	    close(sockfd);
	    deflateEnd(&defstream);
	    inflateEnd(&infstream);
	    exit(1);
	  }
	  if (fileName != NULL) {
	    buffer2[charsSent++] = temp;
	  }
	}

	//LOG
	if (fileName != NULL) {
	  buffer2[charsSent] = '\0';
	  dprintf(fd, "SENT %i bytes: %s\n", charsSent, buffer2);
	  free(buffer2);
	}
      }
    }
    
    else if ((inputs[0].revents | inputs[1].revents) & POLLHUP) {
      end = 1;
    }
  }

  if (charsRead < 0) {
    fprintf(stderr, "Error reading: %s\n", strerror(errno));
    free(buffer);
    revertTermAttr();
    close(sockfd);
    deflateEnd(&defstream);
    inflateEnd(&infstream);
    exit(1);
  }

  free(buffer);
  revertTermAttr();
  close(sockfd);
  deflateEnd(&defstream);
  inflateEnd(&infstream);
  exit(0);
  
  return 0;
}

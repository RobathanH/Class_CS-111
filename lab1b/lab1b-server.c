//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0

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
#include "zlib.h"
#include <assert.h>

void formatErr() {
  fprintf(stderr, "Incorrect Format: ./lab1b-server --port=#  \n");
  //TERMINAL SHOULD NOT HAVE BEEN CHANGED AT THIS POINT, so now terminal reversion necessary
  exit(1);
}

void optionParser(int argc, char **argv, int *portPtr, char **shellNamePtr, int *compressPtr) {
  //Argument Checker
  char *optstring = "";
  int *longindex = NULL;

  struct option longopts[4];
  longopts[0].name = "port";
  longopts[0].has_arg = 1;
  longopts[0].flag = NULL;
  longopts[0].val = 'p';
  longopts[1].name = "shell";
  longopts[1].has_arg = 1;
  longopts[1].flag = NULL;
  longopts[1].val = 's';
  longopts[2].name = "compress";
  longopts[2].has_arg = 0;
  longopts[2].flag = NULL;
  longopts[2].val = 'c';
  longopts[3].name = NULL;
  longopts[3].has_arg = 0;
  longopts[3].flag = NULL;
  longopts[3].val = 0;
  
  
  opterr = 0;

  //Default vals
  *portPtr = -1;
  *shellNamePtr = NULL;
  *compressPtr = 0;

  int val;
  while ((val = getopt_long(argc, argv, optstring, longopts, longindex)) != -1) {
    switch(val) {
    case 'p':
      if (*portPtr != -1) formatErr(); //Since port has already been set once

      //Test that port num is valid
      int i;
      if (optarg != 0) {
	for (i = 0; optarg[i] != '\0'; i++)
	  if (!isdigit(optarg[i])) formatErr();
	*portPtr = atoi(optarg);
      }
      break;

    case 's':
      if (*shellNamePtr != NULL) formatErr();
      *shellNamePtr = optarg;
      break;

    case 'c':
      if (*compressPtr != 0) formatErr();
      *compressPtr = 1;
      break;
      
    default:
      formatErr();
    }
  }

  //Check that mandatory --port arg has been given
  if (*portPtr == -1) formatErr();
}

//Returns file descriptor of socket
int setupSocket(int port) {
  
  //Constants
  int maxQueueSize = 3;
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
    close(sockfd);
    exit(1);
  }

  if (bind(sockfd, (struct sockaddr *) &address, sizeof(address)) != 0) {
    fprintf(stderr, "Error binding socket to address: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }

  if (listen(sockfd, maxQueueSize) != 0) {
    fprintf(stderr, "Error listening for connection requests: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }

  int new_fd = accept(sockfd, NULL, 0);
  if (new_fd < 0) {
    fprintf(stderr, "Error accepting connection request: %s\n", strerror(errno));
    close(sockfd);
    exit(1);
  }
  close(sockfd); //close listening socket

  return new_fd;
}


//Returns pid of shell
int forkShell(char *shellName, int sockfd, int *fdToShell, int *fdToServ) {
  
  if (pipe(fdToShell) < 0 || pipe(fdToServ) < 0) {
    fprintf(stderr, "Error creating pipes to shell: %s", strerror(errno));
    exit(1);
  }

  int pid;
  if (!(pid = fork())) {//child
    //redirect stdout and stderr
    close(1);
    close(2);
    dup(fdToServ[1]);
    dup(fdToServ[1]);
    
    //redirect stdin
    close(0);
    dup(fdToShell[0]);

    //close extra file descriptors
    close(fdToServ[1]);
    close(fdToServ[0]);
    close(fdToShell[1]);
    close(fdToShell[0]);
    close(sockfd);

    char *args[] = {shellName, NULL};
    execvp(args[0], args);
  }

  
  //for parent
  close(fdToServ[1]);
  close(fdToShell[0]);

  return pid;
}






int main(int argc, char **argv) {
  
  int port;
  char *shellName;
  int compress;
  optionParser(argc, argv, &port, &shellName, &compress);

  if (shellName == 0) {//No --shell option used, revert to default
    shellName = "/bin/sh";
  }
  
  //Set up socket
  int sockfd = setupSocket(port);


  //Set up shell and create pipes
  int fdToShell[2];
  int fdToServ[2];
  int shellPID;
  
  shellPID = forkShell(shellName, sockfd, fdToShell, fdToServ);


  //Polling structure
  struct pollfd inputs[2];
  inputs[0].fd = sockfd;
  inputs[0].events = POLLIN;
  inputs[1].fd = fdToServ[0];
  inputs[1].events = POLLIN;

  //Compression vars
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
  int BUFSIZE = 256; //# of characters in buffer
  int TIMEOUT = 1000; //ms for polling to wait for fd to become ready
  char *buffer = malloc(BUFSIZE * sizeof(char));
  
  int charsRead = 0;
  int end = 0;

  while (!end && charsRead >= 0) {
    int ret;
    while ((ret = poll(inputs, 2, TIMEOUT)) == 0);

    if (ret < 0) {//ERROR
      fprintf(stderr, "Error polling input streams: %s\n", strerror(errno));
      free(buffer);
      close(sockfd);
      close(fdToServ[0]);
      close(fdToShell[1]);
      deflateEnd(&defstream);
      inflateEnd(&infstream);
      exit(1);
    }

    if ((inputs[0].revents | inputs[1].revents) & POLLERR) {
      fprintf(stderr, "Error in polling.\n");
      free(buffer);
      close(sockfd);
      close(fdToServ[0]);
      close(fdToShell[1]);
      deflateEnd(&defstream);
      inflateEnd(&infstream);
      exit(1);
    }

    if (inputs[0].revents & POLLIN) {//INput from sockfd
      if ((charsRead = read(sockfd, buffer, BUFSIZE)) > 0) {
	
	//Compression - inflate/decompress
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
	  case 3:
	    kill(shellPID, SIGINT);
	    break;

	  case 4:
	    close(fdToShell[1]);
	    break;

	  default:
	    if (write(fdToShell[1], &temp, 1) < 1) {
	      fprintf(stderr, "Error writing to shell: %s\n", strerror(errno));
	      free(buffer);
	      close(sockfd);
	      close(fdToServ[0]);
	      close(fdToShell[1]);
	      deflateEnd(&defstream);
	      inflateEnd(&infstream);
	      exit(1);
	    }
	  }
	}
      }

    }
    else if (inputs[1].revents & POLLIN) {//input from fdToServ[0]
      if ((charsRead = read(fdToServ[0], buffer, BUFSIZE)) > 0) {
	//Search for ^D
	int i, maxInd = -1;
	for (i = 0; i < charsRead && maxInd == -1; i++) {
	  if (buffer[i] == (char) 4) {
	    maxInd = i;
	  }
	}
	
	//Compression - deflate/compress
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

	//Send chars over socket
	for (i = 0; i < charsRead && i != maxInd; i++) {
	  char temp = buffer[i];

	  if (write(sockfd, &temp, 1) < 1) {
	    fprintf(stderr, "Error writing to client: %s\n", strerror(errno));
	    free(buffer);
	    close(sockfd);
	    close(fdToServ[0]);
	    close(fdToShell[1]);
	    deflateEnd(&defstream);
	    inflateEnd(&infstream);
	    exit(1);
	  }
	}

	//Check for EOF from shell
	if (i == maxInd)
	  end = 1;
	
      }
      if (charsRead == 0) {
	kill(shellPID, SIGINT);
	end = 1;
      }
    }
    else if ((inputs[0].revents | inputs[1].revents) & POLLHUP) {
      end = 1;
    }
  }
  if (charsRead < 0) {
    fprintf(stderr, "Error reading input: %s\n", strerror(errno));
    free(buffer);
    close(sockfd);
    close(fdToServ[0]);
    close(fdToShell[1]);
    deflateEnd(&defstream);
    inflateEnd(&infstream);
    exit(1);
  }

  //Shell exit status
  int status; 
  waitpid(shellPID, &status, 0);
  fprintf(stderr, "SHELL EXIT SIGNAL=%i STATUS=%i\n", 0x7f & status, (0xff00 & status) >> 8);

  free(buffer);
  close(sockfd);
  close(fdToServ[0]);
  close(fdToShell[1]);
  deflateEnd(&defstream);
  inflateEnd(&infstream);
  exit(0);
  }

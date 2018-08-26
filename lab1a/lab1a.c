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


int main(int argc, char **argv) {
  //CONSTANTS
  int fIn = 0; //keyboard input
  int fOut = 1; //standard output
  int BUFSIZE = 256; //buffer size in chars
  int fdToShell[2]; //pipe fd
  int fdToTerm[2]; //pipe fd
  int shellOpt = 0; //boolean for if shell option has been taken
  int TIMEOUT = 1000; //number of seconds to wait for a file descriptor to become ready
  int pid; //pid of shell - if shell option is used


  //Ignore sigpipe
  signal(SIGPIPE, SIG_IGN);
  
  //Check for command-line option
  if (argc == 2 && strcmp(argv[1], "--shell") == 0) {
    if (pipe(fdToShell) < 0 || pipe(fdToTerm) < 0) {
      fprintf(stderr, "Error creating pipe: %s", strerror(errno));
      exit(1);
    }

    shellOpt = 1;
    
    if (!(pid = fork())) {
      close(fOut);
      dup(fdToTerm[1]);
      
      close(fIn);
      dup(fdToShell[0]);

      close(fdToShell[0]);
      close(fdToShell[1]);
      close(fdToTerm[0]);
      close(fdToTerm[1]);
      
      char *args[] = {"/bin/bash", NULL};
      execvp(args[0], args);
    }
    else {
      close(fdToTerm[1]);
      close(fdToShell[0]);
    }
  }
  else if (argc > 1) {
    fprintf(stderr, "Format Error: Correct usage is ./lab1a [--shell]");
    exit(1);
  }
  
  
  struct termios *old = malloc(sizeof(struct termios));
  tcgetattr(fIn, old);

  //change terminal settings
  struct termios *new = malloc(sizeof(struct termios));
  *new = *old;
  new->c_iflag = ISTRIP;
  new->c_oflag = 0;
  new->c_lflag = 0;

  //effect new terminal settings
  tcsetattr(fIn, TCSANOW, new);
  free(new);
  
  
  //Polling structure - if shell created
  struct pollfd *inputs;
  if (shellOpt) {
    inputs = malloc(2 * sizeof(struct pollfd));
    inputs[0].fd = fIn;
    inputs[0].events = POLLIN;
    inputs[1].fd = fdToTerm[0];
    inputs[1].events = POLLIN;
  }
  
  //READING
  char *buffer = malloc(BUFSIZE * sizeof(char));
  
  int charsRead;
  int end = 0;

  if (shellOpt) {
    while (!end) {
      int ret;
      //poll continuously
      while ((ret = poll(inputs, 2, TIMEOUT)) == 0);

      if (ret < 0) {
	fprintf(stderr, "Error polling input streams: %s", strerror(errno));

	if (shellOpt)
	  free(inputs);
  
	tcsetattr(fIn, TCSANOW, old);
	free(old);
	exit(1);
      }
      
      else {
	//Shell output as input
	if (inputs[1].revents & POLLIN) {
	  if ((charsRead = read(fdToTerm[0], buffer, BUFSIZE)) > 0) {
	    int i;
	    for (i = 0; i < charsRead && !end; i++) {
	      char temp = buffer[i];
	      switch(temp) {
	      case 10: //Line feed
	      case 13: //Carriage Return
		temp = 13;
		char temp2 = 10;
		if (write(fOut, &temp, 1) < 1 || write(fOut, &temp2, 1) < 1) {
		  fprintf(stderr, "Error writing to file stream %i: %s", fOut, strerror(errno));

		  if (shellOpt)
		    free(inputs);
  
		  tcsetattr(fIn, TCSANOW, old);
		  free(old);
		  exit(1);
		}
		break;
	
	      case 4:
		end = 1;
		break;
	
	      default:
		if (write(fOut, &temp, 1) < 1) {
		  fprintf(stderr, "Error writing to file stream %i: %s", fOut, strerror(errno));

		  if (shellOpt)
		    free(inputs);
	  
		  tcsetattr(fIn, TCSANOW, old);
		  free(old);
		  exit(1);
		}
	      }
	    }
	  }
	}
	
	//KEYBOARD input
	if (inputs[0].revents & POLLIN) {
	  if ((charsRead = read(fIn, buffer, BUFSIZE)) > 0) {
	    int i;
	    for (i = 0; i < charsRead && !end; i++) {
	      char temp = buffer[i];
	      switch(temp) {
	      case 10: //Line feed
	      case 13: //Carriage Return
		temp = 10;
		char temp2 = 13;
		if (write(fOut, &temp2, 1) < 1 || write(fOut, &temp, 1) < 1 || write(fdToShell[1], &temp, 1) < 1) {
		  fprintf(stderr, "Error writing to file stream %i: %s", fdToShell[1], strerror(errno));

		  if (shellOpt)
		    free(inputs);
  
		  tcsetattr(fIn, TCSANOW, old);
		  free(old);
		  exit(1);
		}
		break;
	      case 3:
		kill(pid, SIGINT);
		break;
	      
	      case 4:
		close(fdToShell[1]);
		break;
	
	      default:
		if (write(fOut, &temp, 1) < 1 || write(fdToShell[1], &temp, 1) < 1) {
		  fprintf(stderr, "Error writing to file stream %i: %s", fdToShell[1], strerror(errno));

		  if (shellOpt)
		    free(inputs);
	  
		  tcsetattr(fIn, TCSANOW, old);
		  free(old);
		  exit(1);
		}
	      }
	    }
	  }
	}
	else if ((inputs[0].revents | inputs[1].revents) & POLLERR) {
	  fprintf(stderr, "Error in polling.");

	  if (shellOpt)
	    free(inputs);
	  
	  tcsetattr(fIn, TCSANOW, old);
	  free(old);
	  exit(1);
	}
	else if ((inputs[0].revents | inputs[1].revents) & POLLHUP) {
	  end = 1;
	}
      }
    }
  }
  else {
    if ((charsRead = read(fIn, buffer, BUFSIZE)) <= 0)
      end = 1;
  
    while(!end) {

      int i;
      for (i = 0; i < charsRead && !end; i++) {
	char temp = buffer[i];
	switch(temp) {
	case 10: //Line feed
	case 13: //Carriage Return
	  temp = 13;
	  char temp2 = 10;
	  if (write(fOut, &temp, 1) < 1 || write(fOut, &temp2, 1) < 1) {
	    fprintf(stderr, "Error writing to file stream %i: %s", fOut, strerror(errno));

	    if (shellOpt)
	      free(inputs);
  
	    tcsetattr(fIn, TCSANOW, old);
	    free(old);
	    exit(1);
	  }
	  break;
	
	case 4:
	  end = 1;
	  break;
	
	default:
	  if (write(fOut, &temp, 1) < 1) {
	    fprintf(stderr, "Error writing to file stream %i: %s", fOut, strerror(errno));

	    if (shellOpt)
	      free(inputs);
	  
	    tcsetattr(fIn, TCSANOW, old);
	    free(old);
	    exit(1);
	  }
	}
      }

      if (!end && (charsRead = read(fIn, buffer, BUFSIZE)) <= 0)
	end = 1;
    }
  }


  //ERROR CHECKING
  if (charsRead < 0) {
    fprintf(stderr, "Error reading input: %s", strerror(errno));

    if (shellOpt)
      free(inputs);
  
    tcsetattr(fIn, TCSANOW, old);
    free(old);
    exit(1);
  }

  //SHELL EXIT STATUS
  if (shellOpt) {
    int status;
    waitpid(pid, &status, 0);
    fprintf(stderr, "SHELL EXIT SIGNAL=%i STATUS=%i", 0x7f & status, (0xff00 & status) >> 8);
  }
  
  if (shellOpt)
    free(inputs);
  
  tcsetattr(fIn, TCSANOW, old);
  free(old);
  exit(0);
}





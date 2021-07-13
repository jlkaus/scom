#include <stdio.h>   /* Standard input/output definitions */
#include <sys/types.h> /* system types */
#include <sys/stat.h>  /* system stat stuff */
#include <sys/ioctl.h> /* system ioctls */
#include <stdlib.h>  /* standard library stuff */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <argp.h>    /* arg_parse and co. */

const char *argp_program_version = PKGNAME " " VERSION;
const char *argp_program_bug_address = PKGMAINT";
static char program_documentation[] = PKGNAME " - A very simple interface to communicate over serial lines.\
\vOnce started, commands are excepted at the terminal with a CMD: prompt.  Data read from the serial port \
is displayed in READ: lines, and data sent over the serial port is displayed in WRITE: lines.  Supported commands are:\n\
\txxxxxxxx...\t\tSend hex bytes\n\
\t'asc string\t\tSend ascii bytes\n\
\tq\t\t\tQuit\n\
\t:pathtofile\t\tSend data from given file.\n\
\t>pathtofile\t\tSet file for logging data we recv (just > turns off logging)\n\
\t<pathtofile\t\tSet file for echoing data we send (just < turns off echoing)\n";

static struct argp_option program_options[] = {
  {"device",	'd',"DEVICE",	0,"Use DEVICE as the serial device. Defaults to /dev/ttyUSB0"},
  {"baud",	'b',"BAUDRATE",	0,"Use BAUDRATE as the baud rate of the serial device. Defaults to 38400."},
  {"bits",	't',"BITS",	0,"Use BITS as the size of each character to send and receive. Defaults to 8."},
  {"parity",	'p',"PARITY",	0,"Use PARITY as the parity for the transmissions over the serial device. Only N (none), E (even), and O (odd) are supported. Defaults to N."},
  {"stops",	's',"STOPS",	0,"Use STOPS as the number of stop bits for the transmissions over the serial device. Defaults to 1."},
  {0}
};

static int outpfd;
static int echofd;
static int recvfd;

struct program_arguments {
  char *device;
  int baud;
  int bits;
  char parity;
  int stops;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct program_arguments *args = state->input;

  switch(key) {

  case 'd':
    args->device = arg;
    break;
  case 'b':
    args->baud = atoi(arg);
    break;
  case 't':
    args->bits = atoi(arg);
    break;
  case 'p':
    args->parity = arg[0];
    break;
  case 's':
    args->stops = atoi(arg);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static struct argp program_arg_parser = {program_options, parse_opt, 0, program_documentation};

// Setup the device into the right mode and open the file device and return it if it worked.  Needs to be closed later.
int portSetup(struct program_arguments *args) {
  outpfd = open(args->device,O_RDWR | O_NOCTTY | O_NDELAY);

  if(outpfd != -1) {
    fcntl(outpfd, F_SETFL, FNDELAY);

    struct termios options;
    tcgetattr(outpfd, &options);

    switch(args->baud) {
    case 38400:
      cfsetispeed(&options, B38400);
      cfsetospeed(&options, B38400);
      break;
    case 19200:
      cfsetispeed(&options, B19200);
      cfsetospeed(&options, B19200);
      break;
    case 9600:
      cfsetispeed(&options, B9600);
      cfsetospeed(&options, B9600);
      break;
    case 4800:
      cfsetispeed(&options, B4800);
      cfsetospeed(&options, B4800);
      break;
    case 2400:
      cfsetispeed(&options, B2400);
      cfsetospeed(&options, B2400);
      break;
    default:
      fprintf(stderr, "ERROR: Unsupported baud rate specified. Sorry.  You can add it if you like.\n");
      close(outpfd);
      exit(-1);
    }

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;

    switch(args->bits) {
    case 8:
      options.c_cflag |= CS8;
      break;
    case 7:
      options.c_cflag |= CS7;
      break;
    case 5:
      options.c_cflag |= CS5;
      break;
    default:
      fprintf(stderr, "ERROR: Unsupported number of bits specified. Sorry.  You can add it if you like.\n");
      close(outpfd);
      exit(-1);
    }

    switch(args->parity) {
    case 'N':
      options.c_cflag &= ~PARENB;
      options.c_iflag &= ~(INPCK | ISTRIP);
      break;
    case 'E':
      options.c_cflag |= PARENB;
      options.c_cflag &= ~PARODD;
      options.c_iflag |= (INPCK | ISTRIP);
      break;
    case 'O':
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      options.c_iflag |= (INPCK | ISTRIP);
      break;
    default:
      fprintf(stderr, "ERROR: Unsupported parity specified. Sorry.  You can add it if you like.\n");
      close(outpfd);
      exit(-1);
    }

    switch(args->stops) {
    case 1:
      options.c_cflag &= ~CSTOPB;
      break;
    case 2:
      options.c_cflag |= CSTOPB;
      break;
    default:
      fprintf(stderr, "ERROR: Unsupported stops specified. Sorry.  You can add it if you like.\n");
      close(outpfd);
      exit(-1);
    }

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    tcsetattr(outpfd, TCSANOW, &options);
  }

  return outpfd;
}

// Hex mode helpers
// Display a command prompt, and wait for a command to be entered, then parse it.  If its an unrecognized command, 0 will be returned.
// Commands accepted: (terminated with enter)
//	xxxxxxxx...		Send hex bytes
//	'string			Send ascii bytes
//	q			Quit
//	:pathtofile		Send data from given file.
//	>pathtofile		Set file for logging data we recv (just > turns off logging)
//	<pathtofile		Set file for echoing data we send (just < turns off echoing)
char waitForCommand(char **buffer) {
  char cmd = 0;

  *buffer = NULL;
  size_t size = 0;

  fprintf(stdout, "CMD: ");
  if(getline(buffer, &size, stdin) != -1) {
    (*buffer)[strlen(*buffer)-1]=0;
    cmd = (*buffer)[0];
  }

  return cmd;
}

// Send the specified string over the wire and display a WRITE: line.
void sendBytes(char *data, size_t size) {
  size_t i=0;

  if(write(outpfd, data, size) != size) {
    fprintf(stderr,"ERROR: Jon, take care of this. Its broken. sendBytes() wire write didn't send it all.\n");
    exit(-2);
  }

  fprintf(stdout, "WRITE: ");
  for(i=0;i<size;++i) {
    fprintf(stdout, "%02X", (unsigned char)data[i]);
  }
  fprintf(stdout, "\n");

  if(echofd != -1) {
    if(write(echofd, data, size) != size) {
      fprintf(stderr,"ERROR: Jon, take care of this. Its broken. sendBytes() echo write didn't send it all.\n");
      exit(-2);
    }
  }
}

// Send the specified string, after conversion from ascii characters representing hex bytes to the hex bytes themselves, over the wire. Display a WRITE: line.
void sendHexString(char *hexstring) {
  size_t i =0;
  size_t size = strlen(hexstring)/2;
  unsigned char *dataToSend = malloc(size);

  char temp[3];
  temp[2] = 0;
  for(i=0; i < size; ++i) {
    temp[0]=hexstring[i*2];
    temp[1]=hexstring[i*2+1];
    dataToSend[i] = strtoul(temp,NULL,16);
  }

  sendBytes(dataToSend, size);

  free(dataToSend);
}

// Send the contents of the file over the wire and displayed an abreviated WRITE: line.
void sendFile(char *filename) {
  int infd = open(filename, O_RDONLY);

  if(infd != -1) {
    fprintf(stdout,"WRITE: ");

    size_t count = 0;
    unsigned char * byte = 0;
    while(read(infd, &byte, 1) > 0) {

      if(write(outpfd, &byte, 1) != 1) {
	fprintf(stderr, "ERROR: Jon, take care of this.  Its broken. sendFile() wire write didn't send it all.\n");
	exit(-2);
      }

      if(count < 4) {
	fprintf(stdout, "%02hhX", byte);
      } else if(count % 256 == 0) {
	fprintf(stdout, ".");
      }
      if(echofd != -1) {
	if(write(echofd, &byte, 1) != 1) {
	  fprintf(stderr, "ERROR: Jon, take care of this.  Its broken. sendFile() echo write didn't send it all.\n");
	  exit(-2);
	}
      }
      ++count;
    }
    close(infd);
    infd = -1;

    fprintf(stdout, " (%z)\n", count);
  } else {
    fprintf(stderr,"ERROR: No such file %s\n", filename);
  }
}

// Check to see if there are bytes to read from the wire, and if there are, display a READ: line. Otherwise, do nothing.
void recvHexString() {
  size_t count = 0;
  size_t i = 0;

  ioctl(outpfd, FIONREAD, &count);

  if(count) {
    unsigned char *buffer = malloc(count);

    read(outpfd, buffer, count);

    if(recvfd != -1) {
      write(recvfd, buffer, count);
    }

    fprintf(stdout,"READ: ");

    for(i=0; i < 4 && i < count; ++i) {
      fprintf(stdout, "%02hhX", buffer[i]);
      if((buffer[i] < ' ') || (buffer[i] > '~')) {
	buffer[i] = '.';
      }
    }

    if(count < 64) {
      for(i=4; i < count; ++i) {
	fprintf(stdout, "%02hhX", buffer[i]);
	if((buffer[i] < ' ') || (buffer[i] > '~')) {
	  buffer[i] = '.';
	}
      }
      fprintf(stdout,"  [%.*s]",count, buffer);
    } else {
      fprintf(stdout, "... (%z)  [%.*s]...", count, 4, buffer);
    }

    fprintf(stdout,"\n");

    free(buffer);
  }
}

// Changes the echo file. If filename is null (or an empty string), turns off echoing.  If echoing is on, all bytes that are being sent over the wire get also written to the echo file.
void setEchoFile(char *filename) {
  if(echofd != -1) {
    close(echofd);
    echofd = -1;
  }

  if(filename && strlen(filename)) {
    echofd = open(filename, O_WRONLY|O_CREAT);
    if(echofd == -1) {
      fprintf(stderr, "ERROR: No such file %s\n", filename);
    }
  }
}

// Changes the recv file. If filename is null (or an empty string), turns off the logging.  If logging is on, all bytes that get received from the wire still get displayed, but they also get written to the recv file.
void setRecvFile(char *filename) {
  if(recvfd != -1) {
    close(recvfd);
    recvfd = -1;
  }

  if(filename && strlen(filename)) {
    recvfd = open(filename, O_WRONLY|O_CREAT);
    if(recvfd == -1) {
      fprintf(stderr, "ERROR: No such file %s\n", filename);
    }
  }
}

// the main loop for command entry, data display, and data transmission
void hexModeLoop() {
  char* commanddata = NULL;
  for(;;) {
    sleep(1);
    recvHexString();
    char cmd = waitForCommand(&commanddata);
    recvHexString();

    if(cmd == 0) {
      continue;
    } else if(cmd == 'q') {
      break;
    } else if(cmd == '>') {
      setRecvFile(commanddata+1);
    } else if(cmd == '<') {
      setEchoFile(commanddata+1);
    } else if(cmd == '\'') {
      sendBytes(commanddata+1, strlen(commanddata+1));
    } else if(cmd == ':') {
      sendFile(commanddata+1);
    } else {
      sendHexString(commanddata);
    }

    free(commanddata);
  }
}

int main(int argc, char* argv[]) {
  // interpret arguments
  struct program_arguments args = {"/dev/ttyUSB0", 38400, 8, 'N', 1};
  outpfd = -1;
  echofd = -1;
  recvfd = -1;
  argp_parse(&program_arg_parser, argc, argv, 0,0, &args);

  if(portSetup(&args) != -1) {
    hexModeLoop();
    close(outpfd);
    outpfd = -1;
    if(echofd != -1) {
      close(echofd);
      echofd = -1;
    }
    if(recvfd != -1) {
      close(recvfd);
      recvfd = -1;
    }
  } else {
    fprintf(stderr,"ERROR: Serial port device %s cannot be opened or does not exist.\n", args.device);
    return -1;
  }

  return 0;
}

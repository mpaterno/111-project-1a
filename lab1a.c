#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

struct termios defaultTermState;
struct termios newTermState;
int shellFlag;
int pipeToChild[2];
int pipeToParent[2];
char *shellProgram = NULL;
pid_t pid;
extern int errno;

void noShell_IO()
{
  char buffer[256];
  int readSize = read(STDIN_FILENO, buffer, sizeof(char) * 256);
  while (readSize)
  {
    int i = 0;
    while (i < readSize)
    {
      if (buffer[i] == '\r' || buffer[i] == '\n')
      {
        write(STDOUT_FILENO, '\r', sizeof(char));
        write(STDOUT_FILENO, '\n', sizeof(char));
        printf('Line 33\n');
      }
      else if (buffer[i] == '\4')
      {
        exit(0);
        printf('Line 38\n');
      }
      else
      {
        write(STDOUT_FILENO, buffer[i], sizeof(char));
        printf('Line 42\n');
      }
      i++;
    }
    // May not need memset
    readSize = read(STDIN_FILENO, buffer, sizeof(char) * 256);
  }
}

void initializePipes(int fd1[2], int fd2[2])
{
  if (pipe(fd1) == -1)
  {
    fprintf(stderr, "Pipe creation error.");
    printf('Line 56\n');
    exit(1);
  }
  if (pipe(fd2) == -1)
  {
    fprintf(stderr, "Pipe creation error.");
    printf('Line 62\n');
    exit(1);
  }
}

void shell_IO(int fd1, int fd2)
{
  // poll_IO describing stdin
  struct pollfd pollfds[2];
  pollfds[0].fd = fd1;
  pollfds[1].fd = pipeToParent[0];
  // Both pollfds waiting for either input (POLLIN) or error (POLLHUP, POLLERR) events.
  pollfds[0].events = POLLIN | POLLHUP | POLLERR;
  pollfds[1].events = POLLIN | POLLHUP | POLLERR;

  char buffer[256];
  char current;
  int pollReturn = poll(pollfds, 2, 0);

  if (pollReturn < 0)
  {
    fprintf(stderr, "Return Error\n");
    printf('Line 84\n');
    exit(1);
  }
  if ((pollfds[0].revents & POLLIN))
  {
    int readSize = read(fd1, buffer, sizeof(char) * 256);
    while (readSize)
    {
      int i = 0;
      while (i < readSize)
      {
        if (buffer[i] == '\r' || buffer[i] == '\n')
        {
          write(fd2, '\r', sizeof(char));
          write(fd2, '\n', sizeof(char));
          write(pipeToChild[1], '\n', sizeof(char));
        }
        else if (buffer[i] == '\4')
          close(pipeToChild[1]);
        else if (buffer[i] == '\3')
          kill(pid, SIGINT);
        else
        {
          write(fd2, buffer[i], sizeof(char));
          write(pipeToChild[1], buffer, sizeof(char));
        }
        i++;
      }
      // May not need memset
      readSize = read(STDIN_FILENO, buffer, sizeof(char) * 256);
    }
  }
  // Review below
  if ((POLLERR | POLLHUP) & (pollfds[1].revents))
  {
    exit(0);
  }
}

void shellProcess()
{
  //copy fd's we need to 0 and 1
  dup2(pipeToChild[0], 0);
  dup2(pipeToParent[1], 1);
  //close original versions
  close(pipeToChild[0]);
  close(pipeToParent[1]);

  char path[] = "/bin/bash";
  char *args[2] = {path, NULL};
  if (execvp(shellProgram, args) == -1)
  { //execute shell
    fprintf(stderr, "error: %s", strerror(errno));
    exit(1);
  }
}

void restore()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &defaultTermState);
  if (shellFlag)
  {
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
      fprintf(stderr, "error on waitpid");
      exit(1);
    }
    if (WIFEXITED(status))
    {
      const int es = WEXITSTATUS(status);
      const int ss = WTERMSIG(status);
      fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", ss, es);
      exit(0);
    }
  }
}

int main(int argc, char **argv)
{
  atexit(restore);
  // Store Terminal Attributes to defaultTermState
  tcgetattr(STDIN_FILENO, &defaultTermState);

  // Save for restoration
  tcsetattr(STDIN_FILENO, TCSANOW, &defaultTermState); // Sets new parameters immediately.
  tcgetattr(STDIN_FILENO, &newTermState);
  // Catch error after this step.
  // Set New Attributes
  newTermState.c_iflag = ISTRIP; /* only lower 7 bits	*/
  newTermState.c_oflag = 0;      /* no processing	*/
  newTermState.c_lflag = 0;      /* no processing	*/

  shellFlag = 0;

  static struct option long_options[] =
      {
          {"shell", required_argument, 0, 's'},
      };
  int c;

  // Set Options
  while (1)
  {
    // int option_index = 0;
    c = getopt_long(argc, argv, "s", long_options, NULL);
    /* Detect end of options */
    if (c == -1)
      break;
    else if (c == 's')
    {
      shellFlag = 1;
      if (optarg != NULL)
      {
        shellProgram = optarg;
      }
    }
    else
      exit(99);
  }
  initializePipes(pipeToChild, pipeToParent);

  if (shellFlag)
  {
    pid = fork(); // Create new process and store ID.
    if (pid == 0)
      shellProcess();
    else if (pid > 0)
    {
      close(pipeToChild[0]);
      close(pipeToParent[1]);
      shell_IO(0, 1);
    }
    else
    {
      fprintf(stderr, "error: %s", strerror(errno));
      exit(1);
    }
  }
  noShell_IO();

  // Set up read, be able to read constantly, create a buffer that allows more than one character at a time, should not wait for new line.

  /*
  Create terminal struct.
  Get current terminal state and save it.
  Make changes to that struct and copy it to a new one, using option tcsanow
  Make sure it's configured first. 
  */

  exit(0);
}
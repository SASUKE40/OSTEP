#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
  printf("hello (pid:%d)\n", getpid());
  int rc = fork();
  if (rc < 0)
  {
    fprintf(stderr, "fork failed\n");
    exit(1);
  }
  else if (rc == 0)
  {
    printf("child (pid:%d)\n", getpid());
    printf("hello\n");
  }
  else
  {
    wait(NULL);
    printf("parent of %d (pid:%d)\n", rc, getpid());
    printf("goodbye\n");
  }
  return 0;
}
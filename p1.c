#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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
  }
  else
  {
    printf("parent of %d (pid:%d)\n", rc, getpid());
  }
  return 0;
}
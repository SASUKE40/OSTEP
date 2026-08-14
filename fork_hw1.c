#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  printf("hello (pid:%d)\n", getpid());
  int rc = fork();
  int x = 100;
  printf("before fork: x = %d\n", x);
  if (rc < 0)
  {
    fprintf(stderr, "fork failed\n");
    exit(1);
  }
  else if (rc == 0)
  {
    x = 200;
    printf("child (pid:%d)\n", getpid());
    printf("child sees x = %d\n", x);
  }
  else
  {
    x = 300;
    printf("parent of %d (pid:%d)\n", rc, getpid());
    printf("parent sees x = %d\n", x);
  }
  return 0;
}
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
    int rc_wait_child = wait(NULL);
    printf("child of %d, wait result: %d (pid:%d)\n", rc, rc_wait_child, getpid());
  }
  else
  {
    int rc_waitpid = waitpid(rc, NULL, 0);
    printf("parent of %d, wait result: %d (pid:%d)\n", rc, rc_waitpid, getpid());
  }
  return 0;
}
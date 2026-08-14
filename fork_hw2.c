#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
  printf("hello (pid:%d)\n", getpid());
  int rc = fork();
  int fd = open("./fork_hw2.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
  printf("before fork: fd = %d\n", fd);
  if (rc < 0)
  {
    fprintf(stderr, "fork failed\n");
    exit(1);
  }
  else if (rc == 0)
  {
    printf("child (pid:%d)\n", getpid());
    printf("child sees fd = %d\n", fd);
    int rc_write = write(fd, "hello from child\n", 17);
    printf("child wrote %d bytes to fd = %d\n", rc_write, fd);
  }
  else
  {
    printf("parent of %d (pid:%d)\n", rc, getpid());
    printf("parent sees fd = %d\n", fd);
    int rc_write = write(fd, "hello from parent\n", 18);
    printf("parent wrote %d bytes to fd = %d\n", rc_write, fd);
  }
  return 0;
}
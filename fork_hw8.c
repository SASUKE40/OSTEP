#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
  printf("hello (pid:%d)\n", getpid());

  int fd[2];
  if (pipe(fd) == -1)
  {
    fprintf(stderr, "pipe failed\n");
    exit(1);
  }

  int rc1 = fork();
  if (rc1 < 0)
  {
    fprintf(stderr, "fork failed\n");
    exit(1);
  }
  else if (rc1 == 0)
  {
    close(fd[0]);
    printf("child (pid:%d)\n", getpid());
    write(fd[1], "hello from child 1\n", 19);
    close(fd[1]);
    exit(0); // Do not let this child execute the second fork.
  }

  // Only the original parent reaches this fork.
  int rc2 = fork();
  if (rc2 < 0)
  {
    fprintf(stderr, "fork failed\n");
    exit(1);
  }
  else if (rc2 == 0)
  {
    close(fd[1]);
    char buf[8];
    ssize_t n;
    printf("child (pid:%d)\n", getpid());
    while ((n = read(fd[0], buf, 8)) > 0)
    {
      write(STDOUT_FILENO, buf, n);
    }
    close(fd[0]);
    exit(0);
  }

  // The parent does not use the pipe. Closing both ends also allows the
  // second child to observe EOF after the first child finishes writing.
  close(fd[0]);
  close(fd[1]);
  waitpid(rc1, NULL, 0);
  waitpid(rc2, NULL, 0);

  return 0;
}

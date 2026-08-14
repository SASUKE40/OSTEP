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

    // printf("execvp\n");
    // execvp("ls", (char *[]){"ls", NULL});

    // printf("execlp\n");
    // execlp("ls", "ls", NULL);

    // printf("execv\n");
    // execl("/bin/ls", "ls", NULL);

    printf("execv\n");
    execv("/bin/ls", (char *[]){"ls", NULL});
  }
  return 0;
}
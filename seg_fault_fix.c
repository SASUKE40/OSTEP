#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  char *src = "hello";
  char *dst = malloc(strlen(src) + 1);
  strcpy(dst, src);
  return 0;
}
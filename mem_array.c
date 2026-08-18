#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  int *data = malloc(100 * sizeof(int));
  free(data);
  printf("%d\n", data[1]);
  return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  int *data = malloc(100 * sizeof(int));
  data[100] = 0;
  free(data);
  return 0;
}
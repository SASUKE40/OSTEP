#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  int *data = malloc(sizeof(int));
  data[0] = 42;
  int *new_data = realloc(data, 100 * sizeof(int));
  if (new_data == NULL)
  {
    free(data);
    return 1;
  }
  data = new_data;
  data[99] = 99;
  free(data);
  return 0;
}
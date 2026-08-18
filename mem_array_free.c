#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  int data[100];
  free(&data[1]);
  return 0;
}
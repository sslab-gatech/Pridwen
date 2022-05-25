#include <stdio.h>
#include <time.h>

int
main()
{
  unsigned long start, end;
  start = clock();
  printf("Hello from SGX\n");
  end = clock();
  printf("time: %lu (%lu - %lu)\n", end - start, end, start);

  return 0;
}

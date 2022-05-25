#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
NumSift(long* array,     /* Array of numbers */
        unsigned long i, /* Minimum of array */
        unsigned long j) /* Maximum of array */
{
  unsigned long k;
  long temp; /* Used for exchange */
  while ((i + i) <= j) {
    k = i + i;
    if (k < j) {

      if (array[k] < array[k + 1L]) {
        ++k;
      }
    }
    if (array[i] < array[k]) {
      temp = array[k];
      array[k] = array[i];
      array[i] = temp;
      i = k;
    } else
      i = j + 1;
  }
  return;
}

static void
NumHeapSort(long* array,
            unsigned long bottom, /* Lower bound */
            unsigned long top)    /* Upper bound */
{
  unsigned long temp; /* Used to exchange elements */
  unsigned long i;    /* Loop index */

  /*
  ** First, build a heap in the array
  */
  for (i = (top / 2L); i > 0; --i)
    NumSift(array, i, top);

  printf("check:");
  for (i = 0; i < 10; i++) {
    printf(" %ld", array[i]);
  }
  printf("\n");

  /*
  ** Repeatedly extract maximum from heap and place it at the
  ** end of the array.  When we get done, we'll have a sorted
  ** array.
  */
  for (i = top; i > 0; --i) {
    NumSift(array, bottom, i);
    temp = *array; /* Perform exchange */
    *array = *(array + i);
    *(array + i) = temp;
  }
  return;
}

int
main()
{
  // printf("hello wasm\n");
  char buf[64];
  long* array;
  int i;
  float secs, iterations;

  printf("test 0\n");

  array = (long*)malloc(10 * sizeof(long));

  printf("test 1\n");
  for (i = 0; i < 10; i++) {
    array[i] = 10 - i;
  }
  printf("test 2\n");

  printf("init:");
  for (i = 0; i < 10; i++) {
    printf(" %ld", array[i]);
  }
  printf("\n");

  NumHeapSort(array, 0, 9);

  printf("sort:");
  for (i = 0; i < 10; i++) {
    printf(" %ld", array[i]);
  }
  printf("\n");

  secs = 5.001137;
  iterations = 1389.000000;
  sprintf(buf, "%.13f", secs / iterations);
  //sprintf(buf, "%f", iterations);
  puts(buf);

  return 0;
}

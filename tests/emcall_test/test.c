#include <stdio.h>
#include <string.h>
// uname
#include <sys/utsname.h>
// readv & writev
#include <sys/uio.h>
// open
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
// close
#include <unistd.h>
// time
#include <time.h>
// malloc
#include <stdlib.h>

#ifndef EMSCRIPTEN
#define EMSCRIPTEN 1
#endif

void
test(int num, int lv, int rv)
{
  printf("test syscall %d ...", num);
  if (lv >= rv) {
    printf("pass\n");
  } else {
    printf("fail ... return: %d\n", lv);
  }
}

int
test_localtime()
{
  time_t t;
  struct tm* info;
  printf("test localtime\n");
  time(&t);
  printf("time: %ld\n", t);
  info = localtime(&t);
  printf("struct tm size: %lu\n", sizeof(struct tm));
  printf("[offset] tm_sec: %llu, tm_min: %llu, tm_hour: %llu, tm_mday: %llu, tm_mon: "
         "%llu, tm_year: %llu, tm_wday: %llu, tm_yday: %llu, tm_gmtoff: %llu, tm_isdst: "
         "%llu, tm_zone: %llu\n",
         (uint64_t)&info->tm_sec - (uint64_t)info,
         (uint64_t)&info->tm_min - (uint64_t)info,
         (uint64_t)&info->tm_hour - (uint64_t)info,
         (uint64_t)&info->tm_mday - (uint64_t)info,
         (uint64_t)&info->tm_mon - (uint64_t)info,
         (uint64_t)&info->tm_year - (uint64_t)info,
         (uint64_t)&info->tm_wday - (uint64_t)info,
         (uint64_t)&info->tm_yday - (uint64_t)info,
         (uint64_t)&info->tm_gmtoff - (uint64_t)info,
         (uint64_t)&info->tm_isdst - (uint64_t)info,
         (uint64_t)&info->tm_zone - (uint64_t)info);
  printf(
    "tm_sec: %u, tm_min: %u, tm_hour: %u, tm_mday: %u, tm_mon: %u, tm_year: "
    "%u, tm_wday: %u, tm_yday: %u, tm_gmtoff: %lu, tm_isdst: %u, tm_zone: %u\n",
    info->tm_sec,
    info->tm_min,
    info->tm_hour,
    info->tm_mday,
    info->tm_mon,
    info->tm_year,
    info->tm_wday,
    info->tm_yday,
    info->tm_gmtoff,
    info->tm_isdst,
    info->tm_zone);
  return 0;
}

int
test_gmtime()
{
  time_t t;
  struct tm* info;
  printf("test gmtime\n");
  time(&t);
  printf("time: %ld\n", t);
  info = gmtime(&t);
  printf("struct tm size: %lu\n", sizeof(struct tm));
  printf("[offset] tm_sec: %llu, tm_min: %llu, tm_hour: %llu, tm_mday: %llu, tm_mon: "
         "%llu, tm_year: %llu, tm_wday: %llu, tm_yday: %llu, tm_gmtoff: %llu, tm_isdst: "
         "%llu, tm_zone: %llu\n",
         (uint64_t)&info->tm_sec - (uint64_t)info,
         (uint64_t)&info->tm_min - (uint64_t)info,
         (uint64_t)&info->tm_hour - (uint64_t)info,
         (uint64_t)&info->tm_mday - (uint64_t)info,
         (uint64_t)&info->tm_mon - (uint64_t)info,
         (uint64_t)&info->tm_year - (uint64_t)info,
         (uint64_t)&info->tm_wday - (uint64_t)info,
         (uint64_t)&info->tm_yday - (uint64_t)info,
         (uint64_t)&info->tm_gmtoff - (uint64_t)info,
         (uint64_t)&info->tm_isdst - (uint64_t)info,
         (uint64_t)&info->tm_zone - (uint64_t)info);
  printf(
    "tm_sec: %u, tm_min: %u, tm_hour: %u, tm_mday: %u, tm_mon: %u, tm_year: "
    "%u, tm_wday: %u, tm_yday: %u, tm_gmtoff: %lu, tm_isdst: %u, tm_zone: %u\n",
    info->tm_sec,
    info->tm_min,
    info->tm_hour,
    info->tm_mday,
    info->tm_mon,
    info->tm_year,
    info->tm_wday,
    info->tm_yday,
    info->tm_gmtoff,
    info->tm_isdst,
    info->tm_zone);
  return 0;
}

void
test_malloc()
{
  int *a;
  int i;
  a = (int *) malloc(10 * sizeof(int));
  for (i = 0; i < 10; i++) {
    a[i] = i;
    printf("a[%d] (@%llu): %d\n", i, (uint64_t)&a[i], a[i]);
  }
  free(a);
}

int
main()
{
  test_localtime();
  test_gmtime();
  test_malloc();
  return 0;
}

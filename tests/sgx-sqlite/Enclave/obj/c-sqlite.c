#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char* numbers[] = { "zero",    "one",       "two",      "three",
                                 "four",    "five",      "six",      "seven",
                                 "eight",   "nine",      "ten",      "eleven",
                                 "twelve",  "thirteen",  "fourteen", "fifteen",
                                 "sixteen", "seventeen", "eighteen", "nineteen",
                                 "twenty" };

unsigned
swizzle(unsigned in, unsigned limit)
{
  unsigned out = 0;
  while (limit) {
    out = (out << 1) | (in & 1);
    in >>= 1;
    limit >>= 1;
  }
  return out;
}

#define ITER 1000

//int main()
void enclave_main()
{
  sqlite3* db;
  int rc, i;
  int maxb = 65535;
  char* zErrMsg = 0;
  sqlite3_stmt* stmt;
  unsigned long start, end;
  unsigned x1, x2;

  // Open database
  rc = sqlite3_open("c-test.db", &db);

  if (rc != SQLITE_OK) {
    printf("Can't open database: %s\n", sqlite3_errmsg(db));
    exit(0);
  } else {
    printf("Open database successfully\n");
  }

  // Insert test.
  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE TABLE t1(a INTEGER, b INTEGER, c TEXT);", 0, 0,
               &zErrMsg);
  sqlite3_prepare_v2(db, "INSERT INTO t1 VALUES(?1,?2,?3);", -1, &stmt, NULL);

  start = clock();
  for (i = 1; i <= ITER; i++) {
    x1 = swizzle(i, maxb);
    sqlite3_bind_int(stmt, 1, x1);
    sqlite3_bind_int(stmt, 2, i);
    sqlite3_bind_text(stmt, 3, numbers[i % 10 + 1], -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      printf("insert failled, return %d\n", rc);
      exit(0);
    }
    sqlite3_reset(stmt);
  }
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
  end = clock();
  sqlite3_finalize(stmt);

  printf("%lu (%lu - %lu)\n", end - start, end, start);
  printf("insert done\n");

  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE TABLE t2(a INTEGER UNIQUE, b INTEGER, c TEXT);", 0, 0,
               &zErrMsg);
  sqlite3_prepare_v2(db, "INSERT INTO t2 VALUES(?1,?2,?3);", -1, &stmt, NULL);
  for (i = 1; i <= ITER; i++) {
    x1 = swizzle(i, maxb);
    sqlite3_bind_int(stmt, 1, i);
    sqlite3_bind_int(stmt, 2, x1);
    sqlite3_bind_text(stmt, 3, numbers[i % 10 + 1], -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      printf("insert failled, return %d\n", rc);
      exit(0);
    }
    sqlite3_reset(stmt);    
  }
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
  sqlite3_finalize(stmt);

  sqlite3_exec(db, "ALTER TABLE t2 ADD COLUMN d DEFAULT 123", 0, 0, &zErrMsg);
  sqlite3_exec(db, "SELECT sum(d) FROM t2", 0, 0, &zErrMsg);

#if 0
  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE UNIQUE INDEX t1b ON t1(b);", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE INDEX t1c ON t1(c);", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE UNIQUE INDEX t2b ON t2(b);", 0, 0, &zErrMsg);
  sqlite3_exec(db, "CREATE INDEX t2c ON t2(c DESC);", 0, 0, &zErrMsg);
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
#endif

  // Select test.
  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_prepare_v2(
    db, "SELECT count(*), avg(b), sum(length(c)), group_concat(a) FROM t1\n"
        " WHERE b BETWEEN ?1 AND ?2; -- 10000 times",
    -1, &stmt, NULL);
  start = clock();
  for (i = 1; i <= ITER; i++) {
    // printf("selct: %d\n", i);
    x1 = rand() % maxb;
    x2 = rand() % 10 + 2 + x1;
    sqlite3_bind_int(stmt, 1, x1);
    sqlite3_bind_int(stmt, 2, x2);
    rc = sqlite3_step(stmt);
#if 0 // For debug.
        if (rc == SQLITE_ROW) {
            printf("%d (%u - %u): (%d, %d, %d, %d)\n", i, x1    , x2,
                                            sqlite3_column_int(stmt, 0),
                                            sqlite3_column_int(stmt, 1),
                                            sqlite3_column_int(stmt, 2),
                                            sqlite3_column_int(stmt, 3));
        }
#endif
    sqlite3_reset(stmt);
  }
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
  end = clock();
  sqlite3_finalize(stmt);

  printf("%lu (%lu - %lu)\n", end - start, end, start);
  printf("select done\n");

  // Update
  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_prepare_v2(db, "UPDATE t2 SET d=b*3 WHERE a=?1;", -1, &stmt, NULL);
  start = clock();
  for (i = 1; i <= ITER; i++) {
    x1 = rand() % 10000 + 1;
    sqlite3_bind_int(stmt, 1, x1);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      printf("update failled, return %d\n", rc);
      exit(0);
    }
    sqlite3_reset(stmt);      
  }
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
  end = clock();
  sqlite3_finalize(stmt);

  printf("%lu (%lu - %lu)\n", end - start, end, start);
  printf("update done\n");

  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_prepare_v2(db, "DELETE FROM t2 WHERE b BETWEEN ?1 AND ?2;", -1, &stmt, NULL);
  start = clock();
  for (i = 1; i <= ITER; i++) {
    x1 = rand() % maxb + 1;
    x2 = rand() % 10 + 2 + x1;
    sqlite3_bind_int(stmt, 1, x1);
    sqlite3_bind_int(stmt, 2, x2);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("delete failed, return %d\n", rc);
        exit(0);
    }
    sqlite3_reset(stmt);
  }
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);
  end = clock();
  sqlite3_finalize(stmt);

  printf("%lu (%lu - %lu)\n", end - start, end, start); 
  printf("delete done\n");

  sqlite3_exec(db, "BEGIN", 0, 0, &zErrMsg);
  sqlite3_exec(db, "DROP TABLE IF EXISTS t1", 0, 0, &zErrMsg);
  sqlite3_exec(db, "DROP TABLE IF EXISTS t2", 0, 0, &zErrMsg);
  sqlite3_exec(db, "COMMIT", 0, 0, &zErrMsg);

  sqlite3_close(db);

  printf("sqlite test done.\n");

  //return 0;
}

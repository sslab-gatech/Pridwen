/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "sha1.h"

#include <math.h>

#include "ocall_type.h"
typedef void (*sighandler_t)(int);

#include "sgx_trts.h"
#include "sgx_report.h"

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

//#include "ocall_stub.h"

#include "ocall_stub.c"

#define TSX 0
#define SUM 0
#define FIB 0
#define SHA 0
#define NBD 0
#define FAN 0
#define SGXWASM_RT 0
#define REPORT 1

#define SUM_LOOP 1000000

#if TSX
int check_tsx();
void test_tsx() {
  int ret = check_tsx();
  printf("tsx return: %\d\n", ret);
}
#endif

#if SUM
int sum(int x, int y)
{
  return x + y;
}

void test_sum()
{
  uint64_t t1, t2;
  int i;
  int((*fun)(int, int)) = sum;

  ocall_sgx_rdtsc(&t1);
  for (i = 0; i < SUM_LOOP; i++) {
    //sum(5, 6);
    fun(5, 6);
  }
  ocall_sgx_rdtsc(&t2);
  printf("sum: %lu\n", t2 - t1);
}
#endif

#if FIB
int fib(int n)
{
  if (n == 0 || n == 1) {
    return 1;
  }
  return (fib(n - 1) + fib(n - 2));
}

void test_fib()
{
  uint64_t t1, t2;
  int i;
  int((*fun)(int)) = fib;

  ocall_sgx_rdtsc(&t1);
  for (i = 0; i < SUM_LOOP; i++) {
    fun(10);
  }
  ocall_sgx_rdtsc(&t2);
  printf("fib: %lu\n", t2 - t1);
}
#endif

#if SHA
/* SHA */

/* Test Vector 1 */
void testvec1(
    void
)
{
  char const string[] = "abc";
  char const expect[] = "a9993e364706816aba3e25717850c26c9cd0d89d";
  char result[21];
  char hexresult[41];
  size_t offset;

  /* calculate hash */
  SHA1( result, string, strlen(string) );
}

void sha() {
  int((*fun)()) = testvec1;
  fun();
}

void test_sha()
{
  uint64_t t1, t2;
  int i;
  //int((*fun)()) = testvec1;
  void((*fun)()) = sha;

  ocall_sgx_rdtsc(&t1);
  fun();
  ocall_sgx_rdtsc(&t2);
  printf("sha: %lu\n", t2 - t1);
}
#endif

#if NBD
/* NDODY */

#define pi 3.141592653589793
#define solar_mass (4 * pi * pi)
#define days_per_year 365.24

struct planet {
  double x, y, z;
  double vx, vy, vz;
  double mass;
};

void advance(int nbodies, struct planet * bodies, double dt)
{
  int i, j;

  for (i = 0; i < nbodies; i++) {
    struct planet * b = &(bodies[i]);
    for (j = i + 1; j < nbodies; j++) {
      struct planet * b2 = &(bodies[j]);
      double dx = b->x - b2->x;
      double dy = b->y - b2->y;
      double dz = b->z - b2->z;
      double distance = sqrt(dx * dx + dy * dy + dz * dz);
      double mag = dt / (distance * distance * distance);
      b->vx -= dx * b2->mass * mag;
      b->vy -= dy * b2->mass * mag;
      b->vz -= dz * b2->mass * mag;
      b2->vx += dx * b->mass * mag;
      b2->vy += dy * b->mass * mag;
      b2->vz += dz * b->mass * mag;
    }
  }
  for (i = 0; i < nbodies; i++) {
    struct planet * b = &(bodies[i]);
    b->x += dt * b->vx;
    b->y += dt * b->vy;
    b->z += dt * b->vz;
  }
}

double energy(int nbodies, struct planet * bodies)
{
  double e;
  int i, j;

  e = 0.0;
  for (i = 0; i < nbodies; i++) {
    struct planet * b = &(bodies[i]);
    e += 0.5 * b->mass * (b->vx * b->vx + b->vy * b->vy + b->vz * b->vz);
    for (j = i + 1; j < nbodies; j++) {
      struct planet * b2 = &(bodies[j]);
      double dx = b->x - b2->x;
      double dy = b->y - b2->y;
      double dz = b->z - b2->z;
      double distance = sqrt(dx * dx + dy * dy + dz * dz);
      e -= (b->mass * b2->mass) / distance;
    }
  }
  return e;
}

void offset_momentum(int nbodies, struct planet * bodies)
{
  double px = 0.0, py = 0.0, pz = 0.0;
  int i;
  for (i = 0; i < nbodies; i++) {
    px += bodies[i].vx * bodies[i].mass;
    py += bodies[i].vy * bodies[i].mass;
    pz += bodies[i].vz * bodies[i].mass;
  }
  bodies[0].vx = - px / solar_mass;
  bodies[0].vy = - py / solar_mass;
  bodies[0].vz = - pz / solar_mass;
}

#define NBODIES 5
struct planet bodies[NBODIES] = {
  {                               /* sun */
    0, 0, 0, 0, 0, 0, solar_mass
  },
  {                               /* jupiter */
    4.84143144246472090e+00,
    -1.16032004402742839e+00,
    -1.03622044471123109e-01,
    1.66007664274403694e-03 * days_per_year,
    7.69901118419740425e-03 * days_per_year,
    -6.90460016972063023e-05 * days_per_year,
    9.54791938424326609e-04 * solar_mass
  },
  {                               /* saturn */
    8.34336671824457987e+00,
    4.12479856412430479e+00,
    -4.03523417114321381e-01,
    -2.76742510726862411e-03 * days_per_year,
    4.99852801234917238e-03 * days_per_year,
    2.30417297573763929e-05 * days_per_year,
    2.85885980666130812e-04 * solar_mass
  },
  {                               /* uranus */
    1.28943695621391310e+01,
    -1.51111514016986312e+01,
    -2.23307578892655734e-01,
    2.96460137564761618e-03 * days_per_year,
    2.37847173959480950e-03 * days_per_year,
    -2.96589568540237556e-05 * days_per_year,
    4.36624404335156298e-05 * solar_mass
  },
  {                               /* neptune */
    1.53796971148509165e+01,
    -2.59193146099879641e+01,
    1.79258772950371181e-01,
    2.68067772490389322e-03 * days_per_year,
    1.62824170038242295e-03 * days_per_year,
    -9.51592254519715870e-05 * days_per_year,
    5.15138902046611451e-05 * solar_mass
  }
};

void nbody()
{
  int n = 10;
  int i;

  offset_momentum(NBODIES, bodies);
  energy(NBODIES, bodies);
  for (i = 1; i <= n; i++)
    advance(NBODIES, bodies, 0.01);
  energy(NBODIES, bodies);
}

void test_nbody()
{
  uint64_t t1, t2;
  int i;
  int((*fun)()) = nbody;

  ocall_sgx_rdtsc(&t1);
  //for (i = 0; i < SUM_LOOP; i++) {
    fun();
  //}
  ocall_sgx_rdtsc(&t2);
  printf("nbody: %lu\n", t2 - t1);
}
#endif

#if FAN
/* fannkuchredux */

inline static int max(int a, int b)
{
    return a > b ? a : b;
}

int fannkuchredux(int n)
{
    int perm[n];
    int perm1[n];
    int count[n];
    int maxFlipsCount = 0;
    int permCount = 0;
    int checksum = 0;

    int i;

    for (i=0; i<n; i+=1)
        perm1[i] = i;
    int r = n;

    while (1) {
        while (r != 1) {
            count[r-1] = r;
            r -= 1;
        }

        for (i=0; i<n; i+=1)
            perm[i] = perm1[i];
        int flipsCount = 0;
        int k;

        while ( !((k = perm[0]) == 0) ) {
            int k2 = (k+1) >> 1;
            for (i=0; i<k2; i++) {
                int temp = perm[i]; perm[i] = perm[k-i]; perm[k-i] = temp;
            }
            flipsCount += 1;
        }

        maxFlipsCount = max(maxFlipsCount, flipsCount);
        checksum += permCount % 2 == 0 ? flipsCount : -flipsCount;

        /* Use incremental change to generate another permutation */
        while (1) {
            if (r == n) {
                //printf("%d\n", checksum);
                return maxFlipsCount;
            }

            int perm0 = perm1[0];
            i = 0;
            while (i < r) {
                int j = i + 1;
                perm1[i] = perm1[j];
                i = j;
            }
            perm1[r] = perm0;
            count[r] = count[r] - 1;
            if (count[r] > 0) break;
            r++;
        }
        permCount++;
    }
}

void test_fannkuchredux()
{
  uint64_t t1, t2;
  int i;
  int((*fun)(int)) = fannkuchredux;

  ocall_sgx_rdtsc(&t1);
  //for (i = 0; i < SUM_LOOP; i++) {
    fun(7);
  //}
  ocall_sgx_rdtsc(&t2);
  printf("fannkuchredux: %lu\n", t2 - t1);
}
#endif

#if SGXWASM_RT
int sgxwasm_rt()
{
  char *m, *n = "str";
  m = malloc(100);
  memset(m, 0, 100);
  memcpy(m, n, strlen(n));
  free(m);
  m = calloc(1, 100);
}
#endif

void
dump_bytes(uint64_t addr, size_t offset, size_t n)
{
  uint8_t *s = (uint8_t *)(addr + offset);
  size_t i;
  printf("%u: ", offset);
  for (i = 0; i < n; i++) {
    uint8_t byte = s[i];
      printf("%d ", byte);
  }
  printf("    \n");
}

void enclave_main()
{
#if TSX
  test_tsx();
#endif

#if SUM
  test_sum();
#endif

#if FIB
  //printf("fib(10): %d\n", fib(10));
  test_fib();
#endif

#if SHA
  test_sha();
#endif

#if NBD
  test_nbody();
#endif

#if FAN
  test_fannkuchredux();
#endif

#if SGXWASM_RT
  sgxwasm_rt();
#endif

#if REPORT
  sgx_report_t report;
  sgx_create_report(NULL, NULL, &report);
  printf("get report\n");
  printf("CPUSVN: %lu\n", report.body.cpu_svn);
  printf("MISCELECT: %u\n", report.body.misc_select);
  printf("ISVEXT_PROD_ID: %lu\n", report.body.isv_ext_prod_id);
  printf("ATTRIBUTES: %lx\n", report.body.attributes);
  printf("CONFIG_ID: %lu\n", report.body.config_id);
  printf("ISV_PROD_ID: %lu\n", report.body.isv_prod_id);
  printf("ISVSVN: %u\n", report.body.isv_svn);
  printf("CONFIG_SVN: %u\n", report.body.config_svn);
  printf("ISV_FAMILLY_ID: %u\n", report.body.isv_family_id);

  printf("===DUMP REPORT RAW===\n");
  dump_bytes((uint64_t)&report, 0, 16);
  dump_bytes((uint64_t)&report, 16, 4);
  dump_bytes((uint64_t)&report, 20, 12);
  dump_bytes((uint64_t)&report, 32, 12);
  dump_bytes((uint64_t)&report, 48, 16);
  dump_bytes((uint64_t)&report, 64, 32);
  dump_bytes((uint64_t)&report, 96, 32);
  dump_bytes((uint64_t)&report, 128, 32);
  dump_bytes((uint64_t)&report, 160, 32);
  dump_bytes((uint64_t)&report, 192, 64);
  dump_bytes((uint64_t)&report, 256, 2);
  dump_bytes((uint64_t)&report, 258, 2);
  dump_bytes((uint64_t)&report, 260, 2);
  dump_bytes((uint64_t)&report, 262, 42);
  dump_bytes((uint64_t)&report, 304, 16);
#endif

  asm("l0: xbegin l1; xend; l1:");

  return;
}


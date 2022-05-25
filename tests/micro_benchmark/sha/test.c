/*
SHA1 tests by Philip Woolford <woolford.philip@gmail.com>
100% Public Domain
 */

#include "sha1.h"
#include "stdio.h"
#include "string.h"
//#include "assert.h"

/* Test Vector 1 */
void testvec1(
    void
)
{
  char const string[] = "abc";
  //char const expect[] = "a9993e364706816aba3e25717850c26c9cd0d89d";
  char result[21];
  char hexresult[41];
  size_t offset;

  /* calculate hash */
  SHA1( result, string, strlen(string) );

  /* format the hash for comparison */
  /*for( offset = 0; offset < 20; offset++) {
    sprintf( ( hexresult + (2*offset)), "%02x", result[offset]&0xff);
  }
  assert( strncmp(hexresult, expect, 40) == 0 );*/
}

int main(void)
{
  testvec1();  

  return 0;
}

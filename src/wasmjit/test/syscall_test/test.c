#include <stdio.h>
#include <string.h>
// uname
#include <sys/utsname.h>
// readv & writev
#include <sys/uio.h>
// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// close
#include <unistd.h>

void test(int num, int lv, int rv)
{
  printf("test syscall %d ...", num);
  if (lv >= rv) {
    printf("pass\n");
  } else {
    printf("fail ... return: %d\n", lv);
  }
}

int test_syscall_122()
{
  int ret;
  struct utsname buf;

  ret = uname(&buf);
  printf("sysname: %s (@%ld)\n", buf.sysname, (long)buf.sysname);
  printf("nodename: %s (@%ld)\n", buf.nodename, (long)buf.nodename);
  printf("release: %s (@%ld)\n", buf.release, (long)buf.release);
  printf("version: %s (@%ld)\n", buf.version, (long)buf.version);
  printf("machine: %s (@%ld)\n", buf.machine, (long)buf.machine);
  return ret;
}

int test_syscall_145()
{
  struct iovec recv_iov[1];
  char recv_buf[64] = { 0 };
  int fd;
  int ret;

  printf("flags: %d, mode: %d\n", O_RDONLY, 0644);
  if ((fd = open("tmp", O_RDONLY, 0644)) < 0) {
    printf("open failed\n");
    return -1;
  }
  recv_iov[0].iov_base = recv_buf;
  recv_iov[0].iov_len = sizeof(recv_buf);
  if ((ret = readv(fd, recv_iov, 1)) < 0) {
    printf("readv failed, return %d\n", ret);
    return -1;
  }
  printf("readv: %s\n", (char *)recv_iov[0].iov_base);
  close(fd);
  return ret;
}

int test_syscall_146()
{
  struct iovec send_iov[1];
  char *send_buf = "test writev & readv";
  int fd;
  int ret;
  printf("flags: %d, mode: %d\n", O_CREAT | O_WRONLY, 0644);
  if ((fd = open("tmp", O_CREAT | O_WRONLY, 0644)) < 0) {
    printf("open failed\n");
    return -1;
  }
  send_iov[0].iov_base = send_buf;
  send_iov[0].iov_len = strlen(send_buf);
  if ((ret = writev(fd, send_iov, 1)) < 0) {
    printf("writev failed\n");
    return -1;
  }
  close(fd);
  return ret;
}

int main()
{
  test(122, test_syscall_122(), 0);
  test(146, test_syscall_146(), 0);
  test(145, test_syscall_145(), 0);

  return 0;
}

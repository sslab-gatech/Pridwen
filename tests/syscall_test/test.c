#include <stdio.h>
#include <string.h>
#include <inttypes.h>
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
test_syscall_5()
{
  int fd;
  int32_t flags;
  
  flags = O_SYNC;
  printf("try open with flags O_SYNC: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);

  flags = O_CREAT;
  printf("try open with flags O_CREAT: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);

  flags = O_RDWR;
  printf("try open with flags O_RDWR: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);

  flags = O_RDONLY;
  printf("try open with flags O_RDONLY: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);
  flags = O_EXCL;

  printf("try open with flags O_EXCL: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);

  flags = O_NOCTTY;
  printf("try open with flags O_NOCTTY: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);

  flags = O_NONBLOCK;
  printf("try open with flags O_NONBLOCK: %u\n", flags);
  fd = open("tmp", flags, 0);
  close(fd);
  return 0;
}

int
test_syscall_122()
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

int
test_syscall_145()
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
  printf("readv: %s\n", (char*)recv_iov[0].iov_base);
  close(fd);
  return ret;
}

int
test_syscall_146()
{
  struct iovec send_iov[1];
  char* send_buf = "test writev & readv";
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

int
test_syscall_140()
{
  char buf[64] = { 0 };
  int fd;
  int ret;

  if ((fd = open("test.txt", O_RDONLY)) < 0) {
    printf("open failed\n");
    return -1;
  }

  ret = lseek(fd, 6, SEEK_CUR);
  read(fd, buf, 5);

  printf("read: %s, ret: %d\n", buf, ret);

  close(fd);
  return ret;
}

/*
struct stat
{
  dev_t st_dev;
  int __st_dev_padding;
  long __st_ino_truncated;
  mode_t st_mode;
  nlink_t st_nlink;
  uid_t st_uid;
  gid_t st_gid;
  dev_t st_rdev;
  int __st_rdev_padding;
  off_t st_size;
  blksize_t st_blksize;
  blkcnt_t st_blocks;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  ino_t st_ino;
};
*/

int
test_syscall_195()
{
  struct stat statbuf;
  int ret;

  ret = stat("test.txt", &statbuf);
  printf("st_dev: %u\nst_ino: %u\nst_nlink: %lu\nst_mode: %u\nst_uid: "
         "%u\nst_gid: %u\nst_rdev: %u\n"
         "st_size: %u\nst_blksize: %ld\nst_blocks: %u\n",
         statbuf.st_dev,
         statbuf.st_ino,
         statbuf.st_nlink,
         statbuf.st_mode,
         statbuf.st_uid,
         statbuf.st_gid,
         statbuf.st_rdev,
         statbuf.st_size,
         statbuf.st_blksize,
         statbuf.st_blocks);
#if EMSCRIPTEN
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atim.tv_sec,
    statbuf.st_atim.tv_nsec,
    statbuf.st_mtim.tv_sec,
    statbuf.st_mtim.tv_nsec,
    statbuf.st_ctim.tv_sec,
    statbuf.st_ctim.tv_nsec);
#if 1
  printf("struct stat (size: %zu), offset:\n", sizeof(struct stat));
  printf(
    "st_dev: %llu\nst_ino: %llu\nst_nlink: %llu\nst_mode: %llu\nst_uid: %llu\nst_gid: %llu\nst_rdev: %llu\n"
    "st_size: %llu\nst_blksize: %llu\nst_blocks: %llu\n",
    (uint64_t)&statbuf.st_dev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ino - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_nlink - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mode - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_uid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_gid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_rdev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_size - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blksize - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blocks - (uint64_t)&statbuf);
  printf(
    "st_atim: %llu\nst_atime_nsec: "
    "%llu\nst_mtim: %llu\nst_mtime_nsec: %llu\nst_ctim: %llu\nst_ctime_nsec: %llu\n",
    (uint64_t)&statbuf.st_atim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_atim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_nsec - (uint64_t)&statbuf);
#endif
#else
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atimespec.tv_sec,
    statbuf.st_atimespec.tv_nsec,
    statbuf.st_mtimespec.tv_sec,
    statbuf.st_mtimespec.tv_nsec,
    statbuf.st_ctimespec.tv_sec,
    statbuf.st_ctimespec.tv_nsec);
#endif
  return ret;
}

int
test_syscall_196()
{
  struct stat statbuf;
  int ret;

  ret = lstat("test.txt", &statbuf);
  printf("st_dev: %u\nst_ino: %u\nst_nlink: %lu\nst_mode: %u\nst_uid: "
         "%u\nst_gid: %u\nst_rdev: %u\n"
         "st_size: %u\nst_blksize: %ld\nst_blocks: %u\n",
         statbuf.st_dev,
         statbuf.st_ino,
         statbuf.st_nlink,
         statbuf.st_mode,
         statbuf.st_uid,
         statbuf.st_gid,
         statbuf.st_rdev,
         statbuf.st_size,
         statbuf.st_blksize,
         statbuf.st_blocks);
#if EMSCRIPTEN
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atim.tv_sec,
    statbuf.st_atim.tv_nsec,
    statbuf.st_mtim.tv_sec,
    statbuf.st_mtim.tv_nsec,
    statbuf.st_ctim.tv_sec,
    statbuf.st_ctim.tv_nsec);
#if 0
  printf(
    "[195]\nst_dev: %llu\nst_ino: %llu\nst_nlink: %llu\nst_mode: %llu\nst_uid: %llu\nst_gid: %llu\nst_rdev: %llu\n"
    "st_size: %llu\nst_blksize: %llu\nst_blocks: %llu\n",
    (uint64_t)&statbuf.st_dev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ino - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_nlink - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mode - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_uid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_gid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_rdev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_size - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blksize - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blocks - (uint64_t)&statbuf);
  printf(
    "st_atim: %llu\nst_atime_nsec: "
    "%llu\nst_mtim: %llu\nst_mtime_nsec: %llu\nst_ctim: %llu\nst_ctime_nsec: %llu\n",
    (uint64_t)&statbuf.st_atim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_atim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_nsec - (uint64_t)&statbuf);
#endif
#else
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atimespec.tv_sec,
    statbuf.st_atimespec.tv_nsec,
    statbuf.st_mtimespec.tv_sec,
    statbuf.st_mtimespec.tv_nsec,
    statbuf.st_ctimespec.tv_sec,
    statbuf.st_ctimespec.tv_nsec);
#endif
  return ret;
}

int
test_syscall_197()
{
  struct stat statbuf;
  int fd;
  int ret;

  fd = open("test.txt", O_RDONLY);
  ret = fstat(fd, &statbuf);
  printf("st_dev: %u\nst_ino: %u\nst_nlink: %lu\nst_mode: %u\nst_uid: "
         "%u\nst_gid: %u\nst_rdev: %u\n"
         "st_size: %u\nst_blksize: %ld\nst_blocks: %u\n",
         statbuf.st_dev,
         statbuf.st_ino,
         statbuf.st_nlink,
         statbuf.st_mode,
         statbuf.st_uid,
         statbuf.st_gid,
         statbuf.st_rdev,
         statbuf.st_size,
         statbuf.st_blksize,
         statbuf.st_blocks);
#if EMSCRIPTEN
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atim.tv_sec,
    statbuf.st_atim.tv_nsec,
    statbuf.st_mtim.tv_sec,
    statbuf.st_mtim.tv_nsec,
    statbuf.st_ctim.tv_sec,
    statbuf.st_ctim.tv_nsec);
#if 0
  printf(
    "[195]\nst_dev: %llu\nst_ino: %llu\nst_nlink: %llu\nst_mode: %llu\nst_uid: %llu\nst_gid: %llu\nst_rdev: %llu\n"
    "st_size: %llu\nst_blksize: %llu\nst_blocks: %llu\n",
    (uint64_t)&statbuf.st_dev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ino - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_nlink - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mode - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_uid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_gid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_rdev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_size - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blksize - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blocks - (uint64_t)&statbuf);
  printf(
    "st_atim: %llu\nst_atime_nsec: "
    "%llu\nst_mtim: %llu\nst_mtime_nsec: %llu\nst_ctim: %llu\nst_ctime_nsec: %llu\n",
    (uint64_t)&statbuf.st_atim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_atim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_nsec - (uint64_t)&statbuf);
#endif
#else
  printf(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atimespec.tv_sec,
    statbuf.st_atimespec.tv_nsec,
    statbuf.st_mtimespec.tv_sec,
    statbuf.st_mtimespec.tv_nsec,
    statbuf.st_ctimespec.tv_sec,
    statbuf.st_ctimespec.tv_nsec);
#endif
  return ret;
}

int
test_syscall_20()
{
  pid_t pid = getpid();
  printf("getpid: %d\n", pid);

  return 0;
}

int
test_syscall_221()
{
  printf("FD_CLOEXEC: %u\n", FD_CLOEXEC);
  printf("O_RDWR|O_NONBLOCK: %u\n", O_RDWR|O_NONBLOCK);
  printf("F_SETFD: %u\n", F_SETFD);
  printf("F_GETFD: %u\n", F_GETFD);
  printf("F_SETFL: %u\n", F_SETFL);
  printf("F_GETFL: %u\n", F_GETFL);
  printf("F_SETLK: %u\n", F_SETLK);
  printf("F_GETLK: %u\n", F_GETLK);

  return 0;
}

int
main()
{
  test(5,   test_syscall_5(), 0);
  test(122, test_syscall_122(), 0);
  test(146, test_syscall_146(), 0);
  test(145, test_syscall_145(), 0);
  test(140, test_syscall_140(), 0);
  test(195, test_syscall_195(), 0);
  test(196, test_syscall_196(), 0);
  test(197, test_syscall_197(), 0);
  test(20, test_syscall_20(), 0);
  test(221, test_syscall_221(), 0);

  return 0;
}

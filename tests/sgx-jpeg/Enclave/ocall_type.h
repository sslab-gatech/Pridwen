#ifndef __OCALL_TYPE_H__
#define __OCALL_TYPE_H__

#define SSIZE_MAX LONG_MAX

typedef void (*sighandler_t)(int);

typedef unsigned long nfds_t; /* Also defined in ../Include/common.h */

// struct timeval { uint8_t byte[16]; };
struct timezone
{
  uint8_t byte[8];
};
// struct sockaddr { uint8_t byte[16]; };
struct servent
{
  uint8_t byte[32];
};
struct protoent
{
  uint8_t byte[24];
};

struct _iobuf
{
  char* _ptr;
  int _cnt;
  char* _base;
  int _flag;
  int _file;
  int _charbuf;
  int _bufsiz;
  char* _tmpfname;
};
typedef struct _iobuf FILE;
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0
#define FILENAME_MAX 260
#define FOPEN_MAX 20
#define _SYS_OPEN 20
#define TMP_MAX 32767
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef short pid_t;

struct flock
{
  short l_type;
  short l_whence;
  off_t l_start;
  off_t l_len;
  pid_t l_pid;
};
#define F_ULOCK 2
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETLK 5
#define F_SETLK 6
#define stdin 0
#define stdout 1
#define stderr 2

struct rlimit
{
  unsigned long rlim_cur;
  unsigned long rlim_max;
};

struct pollfd
{
  int fd;
  short events;
  short revents;
};

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#ifndef POLLWRNORM
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#endif
#ifndef POLLMSG
#define POLLMSG    0x400
#define POLLRDHUP  0x2000
#endif

// sys/select.h
typedef struct {
  uint8_t fd_bits[16];
} fd_set;

#define IOCPARM_MASK 0x1fff

#define IOC_OUT 0x40000000

#define _IOC(inout, group, num, len)                                           \
  (inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))

#define _IOR(g, n, t) _IOC(IOC_OUT, (g), (n), sizeof(t))

#define FIONREAD _IOR('f', 127, int)

/* from fcntl.h */
#define O_RDONLY 00000000
#define O_WRONLY 00000001
#define O_RDWR 00000002
#define O_CREAT 00000100
#define O_EXCL 00000200
#define O_NOCTTY 00000400
#define O_TRUNC 00001000
#define O_APPEND 00002000
#define O_NONBLOCK 00004000
#define O_DSYNC 00010000
#define FASYNC 00020000
#define O_DIRECT 00040000
#define O_LARGEFILE 00100000
#define O_DIRECTORY 00200000
#define O_NOFOLLOW 00400000
#define O_NOATIME 01000000
#define O_CLOEXEC 02000000

/* from bits/alltypes.h */
struct iovec
{
  void* iov_base;
  size_t iov_len;
};
typedef long time_t;
typedef long suseconds_t;
struct timeval
{
  time_t tv_sec;
  suseconds_t tv_usec;
};

// from sys/stat.h
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;

#define S_IFMT  0170000

#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#ifndef S_IRUSR
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007
#endif

#define S_IFMT  0170000

#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000

struct timespec
{
  time_t tv_sec;
  long tv_nsec;
};

struct stat
{
  dev_t st_dev;
  ino_t st_ino;
  nlink_t st_nlink;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
  unsigned int __pad0;
  dev_t st_rdev;
  off_t st_size;
  blksize_t st_blksize;
  blkcnt_t st_blocks;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  long __unused[3];
};

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7
#define CLOCK_REALTIME_ALARM     8
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_SGI_CYCLE         10
#define CLOCK_TAI               11

/* from bits/socket.h */

#define SOCK_STREAM 1

#define AF_UNIX 1
#define AF_UNSPEC 0


typedef unsigned socklen_t;
typedef unsigned short sa_family_t;

struct sockaddr
{
  unsigned short sa_family;
  char sa_data[14];
};

struct in_addr
{
  unsigned int s_addr;
};

struct sockaddr_in
{
  short sin_family;
  unsigned short sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};

struct sockaddr_un {
  sa_family_t sun_family;
  char sun_path[108];
};

struct in_addr6
{
  unsigned char s6_addr[16];
};

struct sockaddr_in6
{
  unsigned short sin6_family;
  unsigned short sin6_port;
  unsigned int sin6_flowinfo;
  struct in_addr6 sin6_addr;
  unsigned int sin6_scope_id;
};

struct sockaddr_storage
{
  unsigned short ss_family;
  char _ss_pad1[6];
  long int _ss_align;
  char _ss_pad2[12];
};

struct addrinfo
{
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  socklen_t ai_addrlen;
  struct sockaddr* ai_addr;
  char* ai_canonname;
  struct addrinfo* ai_next;
};
typedef struct addrinfo addrinfo;

#define AF_INET 2
#define AF_INET6 10
struct msghdr
{
  void* msg_name;
  socklen_t msg_namelen;
  struct iovec* msg_iov;
  int msg_iovlen, __pad1;
  void* msg_control;
  socklen_t msg_controllen, __pad2;
  int msg_flags;
};

struct cmsghdr
{
  socklen_t cmsg_len;
  int __pad1;
  int cmsg_level;
  int cmsg_type;
};

/* from sys/socket.h */
#define SOL_SOCKET 1

#define SCM_RIGHTS 0x01
#define SCM_CREDENTIALS 0x02

#define TCP_NODELAY 1

#define __CMSG_LEN(cmsg)                                                       \
  (((cmsg)->cmsg_len + sizeof(long) - 1) & ~(long)(sizeof(long) - 1))
#define __CMSG_NEXT(cmsg) ((unsigned char*)(cmsg) + __CMSG_LEN(cmsg))
#define __MHDR_END(mhdr)                                                       \
  ((unsigned char*)(mhdr)->msg_control + (mhdr)->msg_controllen)

#define CMSG_DATA(cmsg) ((unsigned char*)(((struct cmsghdr*)(cmsg)) + 1))
#define CMSG_NXTHDR(mhdr, cmsg)                                                \
  ((cmsg)->cmsg_len < sizeof(struct cmsghdr)                                   \
     ? (struct cmsghdr*)0                                                      \
     : (__CMSG_NEXT(cmsg) + sizeof(struct cmsghdr) >= __MHDR_END(mhdr)         \
          ? (struct cmsghdr*)0                                                 \
          : ((struct cmsghdr*)__CMSG_NEXT(cmsg))))
#define CMSG_FIRSTHDR(mhdr)                                                    \
  ((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr)                    \
     ? (struct cmsghdr*)(mhdr)->msg_control                                    \
     : (struct cmsghdr*)0)
#define CMSG_FIRSTHDR(mhdr)                                                    \
  ((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr)                    \
     ? (struct cmsghdr*)(mhdr)->msg_control                                    \
     : (struct cmsghdr*)0)

#define CMSG_ALIGN(len)                                                        \
  (((len) + sizeof(size_t) - 1) & (size_t) ~(sizeof(size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))

struct linger
{
  int l_onoff;
  int l_linger;
};

// netinet/in.h
enum {
  IPPROTO_TCP = 6,
};

typedef uint32_t in_addr_t;
#define INADDR_ANY        ((in_addr_t) 0x00000000)
#define INADDR_BROADCAST  ((in_addr_t) 0xffffffff)
#define INADDR_NONE       ((in_addr_t) 0xffffffff)
#define INADDR_LOOPBACK   ((in_addr_t) 0x7f000001)


#endif

// signal.h
typedef int sig_atomic_t;

// unistd.h
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define _PC_LINK_MAX  0
#define _PC_MAX_CANON 1
#define _PC_MAX_INPUT 2
#define _PC_NAME_MAX  3
#define _PC_PATH_MAX  4
#define _PC_PIPE_BUF  5
#define _PC_CHOWN_RESTRICTED  6
#define _PC_NO_TRUNC  7
#define _PC_VDISABLE  8
#define _PC_SYNC_IO 9
#define _PC_ASYNC_IO  10
#define _PC_PRIO_IO 11
#define _PC_SOCK_MAXBUF 12
#define _PC_FILESIZEBITS  13
#define _PC_REC_INCR_XFER_SIZE  14
#define _PC_REC_MAX_XFER_SIZE 15
#define _PC_REC_MIN_XFER_SIZE 16
#define _PC_REC_XFER_ALIGN  17
#define _PC_ALLOC_SIZE_MIN  18
#define _PC_SYMLINK_MAX 19
#define _PC_2_SYMLINKS  20

// resourec.h
#define RLIMIT_CPU     0
#define RLIMIT_FSIZE   1
#define RLIMIT_DATA    2
#define RLIMIT_STACK   3
#define RLIMIT_CORE    4
#define RLIMIT_NOFILE  7 // musl set 5, should be 7 on glibc
#define RLIMIT_AS      6
#define RLIMIT_RSS     7
#define RLIMIT_NPROC   8
#define RLIMIT_MEMLOCK 9

// stdlib.h
#define WNOHANG    1
#define WUNTRACED  2

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s) ((s) & 0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define WIFEXITED(s) (!WTERMSIG(s))
#define WIFSTOPPED(s) ((short)((((s)&0xffff)*0x10001)>>8) > 0x7f00)
#define WIFSIGNALED(s) (((s)&0xffff)-1U < 0xffu)


// locale.h
#define LC_CTYPE    0
#define LC_NUMERIC  1
#define LC_TIME     2
#define LC_COLLATE  3
#define LC_MONETARY 4
#define LC_MESSAGES 5
#define LC_ALL      6

// syslog.h
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

#ifndef SO_DEBUG
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14
#define SO_REUSEPORT    15
#define SO_PASSCRED     16
#define SO_PEERCRED     17
#define SO_RCVLOWAT     18
#define SO_SNDLOWAT     19
#define SO_RCVTIMEO     20
#define SO_SNDTIMEO     21
#define SO_ACCEPTCONN   30
#define SO_PEERSEC      31
#define SO_SNDBUFFORCE  32
#define SO_RCVBUFFORCE  33
#define SO_PROTOCOL     38
#define SO_DOMAIN       39
#endif

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGIOT    SIGABRT
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   29
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED SIGSYS

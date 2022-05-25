#ifndef __OCALL_TYPE_H__
#define __OCALL_TYPE_H__

typedef void (*sighandler_t)(int);

typedef unsigned long nfds_t; /* Also defined in ../Include/common.h */

//struct timeval { uint8_t byte[16]; };
struct timezone { uint8_t byte[8]; };
struct sockaddr { uint8_t byte[16]; };
struct servent { uint8_t byte[32]; };
struct protoent { uint8_t byte[24]; };
typedef unsigned socklen_t;

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};
typedef struct addrinfo addrinfo;

struct _iobuf {
    char *_ptr;
    int   _cnt;
    char *_base;
    int   _flag;
    int   _file;
    int   _charbuf;
    int   _bufsiz;
    char *_tmpfname;
};
typedef struct _iobuf FILE;
#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_SET    0
#define FILENAME_MAX  260
#define FOPEN_MAX   20
#define _SYS_OPEN   20
#define TMP_MAX 32767
typedef unsigned int mode_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef short pid_t;

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};
#define F_GETFD    1
#define F_SETFD    2
#define F_GETFL    3
#define F_SETFL    4
#define F_GETLK    5
#define F_SETLK    6
#define stdin    0
#define stdout    1
#define stderr    2
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    nlink_t st_nlink;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    unsigned int    __pad0;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    unsigned long st_atim;
    unsigned long st_atime_nsec;
    unsigned long st_mtim;
    unsigned long st_mtime_nsec;
    unsigned long st_ctim;
    unsigned long st_ctime_nsec;
    long __unused[3];
};

struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define IOCPARM_MASK    0x1fff

#define IOC_OUT        0x40000000

#define _IOC(inout, group, num, len) \
        (inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))

#define _IOR(g,n,t)    _IOC(IOC_OUT,    (g), (n), sizeof(t))

#define FIONREAD    _IOR('f', 127, int)

struct in_addr {
    unsigned int s_addr;
};

/* from fcntl.h */
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
#define O_CREAT         00000100
#define O_EXCL          00000200
#define O_NOCTTY        00000400
#define O_TRUNC         00001000
#define O_APPEND        00002000
#define O_NONBLOCK      00004000
#define O_DSYNC         00010000
#define FASYNC          00020000
#define O_DIRECT        00040000
#define O_LARGEFILE     00100000
#define O_DIRECTORY     00200000
#define O_NOFOLLOW      00400000
#define O_NOATIME       01000000
#define O_CLOEXEC       02000000

/* from bits/alltypes.h */
struct iovec { void *iov_base; size_t iov_len; };
typedef long time_t;
typedef long suseconds_t;
struct timeval { time_t tv_sec; suseconds_t tv_usec; };

/* from bits/socket.h */
struct msghdr
{
  void *msg_name;
  socklen_t msg_namelen;
  struct iovec *msg_iov;
  int msg_iovlen, __pad1;
  void *msg_control;
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
#define SOL_SOCKET      1

#define SCM_RIGHTS      0x01
#define SCM_CREDENTIALS 0x02

#define __CMSG_LEN(cmsg) (((cmsg)->cmsg_len + sizeof(long) - 1) & ~(long)(sizeof(long) - 1))
#define __CMSG_NEXT(cmsg) ((unsigned char *)(cmsg) + __CMSG_LEN(cmsg))
#define __MHDR_END(mhdr) ((unsigned char *)(mhdr)->msg_control + (mhdr)->msg_controllen)

#define CMSG_DATA(cmsg) ((unsigned char *) (((struct cmsghdr *)(cmsg)) + 1))
#define CMSG_NXTHDR(mhdr, cmsg) ((cmsg)->cmsg_len < sizeof (struct cmsghdr) ? (struct cmsghdr *)    0 : \
        (__CMSG_NEXT(cmsg) + sizeof (struct cmsghdr) >= __MHDR_END(mhdr) ? (struct cmsghdr *)0 :     \
        ((struct cmsghdr *)__CMSG_NEXT(cmsg))))
#define CMSG_FIRSTHDR(mhdr) ((size_t) (mhdr)->msg_controllen >= sizeof (struct cmsghdr) ? (struct cmsghdr *) (mhdr)->msg_control : (struct cmsghdr *) 0)
#define CMSG_FIRSTHDR(mhdr) ((size_t) (mhdr)->msg_controllen >= sizeof (struct cmsghdr) ? (struct cmsghdr *) (mhdr)->msg_control : (struct cmsghdr *) 0)

#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN (len) + CMSG_ALIGN (sizeof (struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))

struct linger
{
  int l_onoff;
  int l_linger;
};

#endif

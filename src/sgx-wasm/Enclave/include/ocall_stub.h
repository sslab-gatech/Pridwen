#ifndef __OCALL_STUB_H__
#define __OCALL_STUB_H__

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ocall_type.h"

extern char _binary_sgxwasm_code_start; /* defined in the linker script */
extern char _binary_sgxwasm_code_end;
#define __SGXWASM_CODE_BASE ((void*)&_binary_sgxwasm_code_start)
#define __SGXWASM_CODE_END ((void*)&_binary_sgxwasm_code_end)

int rand();
int printf(const char *, ...);
clock_t clock();
time_t time(time_t *);
size_t strftime(char *, size_t, const char *, const struct tm *);

//struct tm localtime;
struct tm *localtime(const time_t *timep);

//struct tm gmtime;
struct tm *gmtime(const time_t *timep);

time_t mktime(struct tm *timeptr);
int gettimeofday(struct timeval *tv, struct timezone *tz);
int puts(const char *str);
int putchar(int c);
//int open(const char *pathname, int flags, unsigned mode);
int open(const char *pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, char * buf, size_t buf_len);
ssize_t write(int fd, const char *buf, size_t n);
off_t lseek(int fildes, off_t offset, int whence);

int socket(int af, int type, int protocol);
int bind(int s, const struct sockaddr *addr, size_t addr_size);
int connect(int s, const struct sockaddr *addr, size_t addrlen);
int listen(int s, int backlog);
int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
int accept4(int s, struct sockaddr *addr, socklen_t *addrlen, int flags);
int fstat(int fd, struct stat *buf);
ssize_t send(int s, const void *buf, size_t len, int flags);
ssize_t recv(int s, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, size_t addrlen);
ssize_t recvfrom(int s, void *buf, size_t len, int flags,
                 struct sockaddr *dest_addr, socklen_t* addrlen);
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
int gethostname(char *name, size_t namelen);

#define INFO_MAX 64
addrinfo addrinfoarr[INFO_MAX];
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, unsigned long *res);

char envret[0x100];
char *getenv(const char *env);
int getsockname(int s, struct sockaddr *name, socklen_t *addrlen);
int getsockopt(int s, int level, int optname, void *optval,
               socklen_t* optlen);
struct servent serventret;
struct servent *getservbyname(const char *name, const char *proto);
struct protoent protoentret;
struct protoent *getprotobynumber(int proto);
int setsockopt(int s, int level, int optname, const void *optval, size_t optlen);
unsigned short htons(unsigned short hostshort);
unsigned long htonl(unsigned long hostlong);
unsigned short ntohs(unsigned short netshort);
unsigned long ntohl(unsigned long netlong);
sighandler_t signal(int signum, sighandler_t a);
int shutdown(int a, int b);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

char *inet_ntop_ret;
char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
char inet_ntoa_ret[200];
char *inet_ntoa(struct in_addr in);
unsigned int inet_addr(const char *cp);
int inet_pton(int ad, const char *src, void *dst);
int mmap();
int munmap();
int syscall();
int clone();
int clock_gettime();
int mremap();

FILE* fopen(const char *filename, const char *mode);
FILE* fdopen(int fd, const char *mode);
int fclose(FILE *stream);
int fseek(FILE *file, long offset, int origin);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
int fprintf(FILE *stream, const char *fmt, ...);
int ferror(FILE *stream);
int access(const char *pathname, int mode);
char* getcwd(char *buf, size_t size);
int stat(const char *pathname, struct stat *statbuf);
int ftruncate(int fd, off_t length);
int fcntl(int fd, int cmd, ...);
int fchmod(int fd, mode_t mode);
int unlink(const char *pathname);
int mkdir(int dirfd, const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int fsync(int fd);
int dup(int oldfd);
int getrlimit(int resource, struct rlimit *rlim);

uid_t geteuid(void);
int ioctl(int fd, unsigned long request, ...);
pid_t getpid();
uid_t getuid();
gid_t getgid();
gid_t getegid();

void exit(int status);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/resource.h>
#include <poll.h>
#include <sys/sendfile.h>

//#define OCALL_TRACE

void ocall_print_string(const char *str)
{
  printf("%s", str);
  //fprintf(stderr, "%s", str);
}

clock_t ocall_sgx_clock()
{
    return clock();
}

time_t ocall_sgx_time(time_t *timep)
{
    return time(timep);
}

struct tm *ocall_sgx_localtime(const time_t *timep)
{
    return localtime(timep);
}

struct tm *ocall_sgx_gmtime(const time_t *timep)
{
    return gmtime(timep);
}

time_t ocall_sgx_mktime(struct tm *timeptr)
{
    return mktime(timeptr);
}

int ocall_sgx_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    return gettimeofday(tv, tz);
}

int ocall_sgx_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
#ifdef OCALL_TRACE
    int ret = clock_gettime(clk_id, tp);
    printf("[clock_gettime] sec: %zu, nsec: %lu\n", tp->tv_sec, tp->tv_nsec);
    return ret;
#else
    return clock_gettime(clk_id, tp);
#endif
}

int ocall_sgx_putchar(int c)
{
    return putchar(c);
}

int ocall_sgx_puts(const char *str)
{
    return puts(str);
}

// skip sgx_push_gadget

int ocall_sgx_open(const char *pathname, int flags, unsigned mode)
{
#ifdef OCALL_TRACE
    int ret;
    ret = open(pathname, flags, mode);
    fprintf(stderr, "open(\"%s\", %d, 0x%x), return: %d\n", pathname, flags, mode, ret);
    return ret;
#else
    return open(pathname, flags, mode);
#endif
}

int ocall_sgx_close(int fd)
{
#ifdef OCALL_TRACE
    int ret;
    ret = close(fd);
    fprintf(stderr, "close(%d), return: %d\n", fd, ret);
    return ret;
#else
    return close(fd);
#endif
}

ssize_t ocall_sgx_read(int fd, char *buf, size_t buf_len)
{
#ifdef OCALL_TRACE
    ssize_t ret;
    ret = read(fd, buf, buf_len);
    fprintf(stderr, "read(%d, %s, %u), return: %ld\n", fd, buf, buf_len, ret);
    return ret;
#else
    return read(fd, buf, buf_len);
#endif
}

ssize_t ocall_sgx_write(int fd, const char *buf, size_t n)
{
    int w = write(fd, buf, n);
    if(w < 0) {
        fprintf(stderr, "Error write!: errno = %d\n", errno);
    }
    return w;
}

off_t ocall_sgx_lseek(int fildes, off_t offset, int whence)
{
    return lseek(fildes, offset, whence);
}

char *ocall_sgx_getcwd(char *buf, size_t size)
{
#ifdef OCALL_TRACE
    char *ret;
    ret = getcwd(buf, size);
    fprintf(stderr, "getcwd(%s, %lu), return: %s\n", buf, size, ret);
    return ret;
#else
    return getcwd(buf, size);
#endif
}

pid_t ocall_sgx_getpid()
{
#ifdef OCALL_TRACE
    pid_t ret;
    ret = getpid();
    fprintf(stderr, "getpid(), return: %d\n", ret);
    return ret;
#else
    return getpid();
#endif
}

uid_t ocall_sgx_getuid()
{
#ifdef OCALL_TRACE
    uid_t ret;
    ret = getuid();
    fprintf(stderr, "getuid(), return: %d\n", ret);
    return ret;
#else
    return getuid();
#endif
}

uid_t ocall_sgx_geteuid()
{
#ifdef OCALL_TRACE
    uid_t ret;
    ret = geteuid();
    fprintf(stderr, "geteuid(), return: %d\n", ret);
    return ret;
#else
    return geteuid();
#endif
}

gid_t ocall_sgx_getgid()
{
#ifdef OCALL_TRACE
    gid_t ret;
    ret = getgid();
    fprintf(stderr, "getgid(), return: %d\n", ret);
    return ret;
#else
    return getgid();
#endif
}

gid_t ocall_sgx_getegid()
{
#ifdef OCALL_TRACE
    gid_t ret;
    ret = getegid();
    fprintf(stderr, "getegid(), return: %d\n", ret);
    return ret;
#else
    return getegid();
#endif
}

int ocall_sgx_fcntl(int fd, int cmd, void *arg, size_t size) {
    int ret;
    switch (cmd) {
    case F_SETFD:
        ret = fcntl(fd, cmd, *(int *)arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_SETFD, %d), return: %d\n", fd, *(int *)arg, ret);
#endif
        break;
    case F_GETFD:
        ret = fcntl(fd, cmd, arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_GETFD, %p), return: %d\n", fd, arg, ret);
#endif
        break;
    case F_SETFL:
        ret = fcntl(fd, cmd, *(int *)arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_SETFL, %d), return: %d\n", fd, *(int *)arg, ret);
#endif
        break;
    case F_GETFL:
        ret = fcntl(fd, cmd, arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_GETFL, %p), return: %d\n", fd, arg, ret);
#endif
        break;
    case F_SETLK:
        ret = fcntl(fd, cmd, arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_SETLK, %p), return: %d\n", fd, arg, ret);
#endif
        break;
    case F_GETLK:
        ret = fcntl(fd, cmd, arg);
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, F_GETLK, %p), return: %d\n", fd, arg, ret);
#endif
        break;
    default:
        ret = 0;
#ifdef OCALL_TRACE
        fprintf(stderr, "fcntl(%d, %d, %p), unsupported cmd!", fd, cmd, arg);
#endif
    }

    return ret;
}

int ocall_sgx_ioctl(int fd, unsigned long request, void *arg, size_t size) {
    int ret;
    switch (request) {
    case FIONREAD:
        ret = ioctl(fd, request, (int *)arg);
#ifdef OCALL_TRACE
        if (arg == 0) {
            fprintf(stderr, "ioctl(%d, FIONREAD, [no args]), return: %d\n", fd, ret);
        } else {
            fprintf(stderr, "ioctl(%d, FIONREAD, %d), return: %d\n", fd, *(int *)arg, ret);
        }
#endif
        break;
    default:
        ret = 0;
#ifdef OCALL_TRACE
        fprintf(stderr, "ioctl(%d, %lu, %p), unsupported cmd!", fd, request, arg);
#endif
    }

    return ret;
}

int ocall_sgx_stat(const char *pathname, struct stat *statbuf) {
#ifdef OCALL_TRACE
    int ret = stat(pathname, statbuf);
    fprintf(stderr, "stat(%s, 0x%p), return: %d\n", pathname, statbuf, ret);
    fprintf(stderr, "st_mode: %07o, uid: %d, gid: %d\n", statbuf->st_mode, statbuf->st_uid, statbuf->st_gid);
    return ret;
#else
    return stat(pathname, statbuf);
#endif
}

int ocall_sgx_fchmod(int fd, mode_t mode) {
#ifdef OCALL_TRACE
    int ret = fchmod(fd, mode);
    fprintf(stderr, "fchmod(%d, %d), return: %d\n", fd, mode, ret);
    return ret;
#else
    return fchmod(fd, mode);
#endif
}

//--------------- network --------------

#define SOCKET int
#define SOCKET_ERROR -1

int ocall_sgx_socket(int af, int type, int protocol)
{
#ifdef OCALL_TRACE
    int ret = socket(af, type, protocol);
    fprintf(stderr, "socket(%d, %d, %d), return: %d\n", af, type, protocol, ret);
    return ret;
#else
    return socket(af, type, protocol);
#endif
}

int ocall_sgx_bind(int s, const struct sockaddr *addr, size_t addr_size)
{
#ifdef OCALL_TRACE
    struct sockaddr_in *in;
    in = (struct sockaddr_in *) addr;
    fprintf(stderr, "family: %d, port: %d, addr: %s\n", in->sin_family, ntohs(in->sin_port), inet_ntoa(in->sin_addr));

    int ret = bind((SOCKET)s, (const struct sockaddr *)addr, addr_size);
    fprintf(stderr, "bind(%d, 0x%p, %lu), return: %d\n", s, addr, addr_size, ret);
    if (ret == -1) {
        fprintf(stderr, "Error code: %d\n", errno);
    }
    return ret;
#else
    return bind((SOCKET)s, (const struct sockaddr *)addr, addr_size);
#endif
}

int ocall_sgx_connect(int s, const struct sockaddr *addr, size_t addrlen)
{
    return connect((SOCKET)s, addr, addrlen);
}

int ocall_sgx_listen(int s, int backlog)
{
#ifdef OCALL_TRACE
    int ret = listen((SOCKET)s, backlog);
    fprintf(stderr, "listen(%d, %d), return: %d\n", s, backlog, ret);
    return ret;
#else
    return listen((SOCKET)s, backlog);
#endif
}

int ocall_sgx_accept(int s, struct sockaddr *addr, unsigned addr_size, socklen_t *addrlen)
{
#ifdef OCALL_TRACE
    int ret = (SOCKET)accept((SOCKET)s, addr, (socklen_t *)addrlen);
    fprintf(stderr, "accept(%d, %p, %lu), return: %d\n", s, addr, *addrlen, ret);
    return ret;
#else
    return (SOCKET)accept((SOCKET)s, addr, (socklen_t *)addrlen);
#endif
}

int ocall_sgx_accept4(int s, struct sockaddr *addr, unsigned addr_size, socklen_t *addrlen, int flags)
{
#ifdef OCALL_TRACE
    int ret = (SOCKET)accept4((SOCKET)s, addr, (socklen_t *)addrlen, flags);
    fprintf(stderr, "accept4(%d, %p, %lu, %d), return: %d\n", s, addr, *addrlen, flags, ret);
    return ret;
#else
    return (SOCKET)accept4((SOCKET)s, addr, (socklen_t *)addrlen, flags);
#endif
}

int ocall_sgx_fstat(int fd, struct stat *buf)
{
#ifdef OCALL_TRACE
    int ret = fstat(fd, buf);
    fprintf(stderr, "fstat(%d, 0x%p), return: %d\n", fd, buf, ret);
    return ret;
#else
    return fstat(fd, buf);
#endif
}

ssize_t ocall_sgx_send(int s, const void *buf, size_t len, int flags)
{
    return send((SOCKET)s, buf, len, flags);
}

ssize_t ocall_sgx_recv(int s, void *buf, size_t len, int flags)
{
    return recv((SOCKET)s, buf, len, flags);
}

ssize_t ocall_sgx_sendto(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, size_t addrlen)
{
    return sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t ocall_sgx_recvfrom(int s, void *buf, size_t len, int flags,
        struct sockaddr *dest_addr, unsigned alen, socklen_t* addrlen)
{
    return recvfrom(s, buf, len, flags, dest_addr, addrlen);
}

int ocall_sgx_gethostname(char *name, size_t namelen)
{
    return gethostname(name, namelen);
}

int ocall_sgx_getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints, unsigned long *res) {
    // TODO: copy res to enclave mem region
    return getaddrinfo(node, service, hints, (struct addrinfo **)res);
}

char *ocall_sgx_getenv(const char *env)
{
    // TODO: copy res to enclave mem region
#ifdef OCALL_TRACE
    char *ret;
    ret = getenv(env);
    if (ret != NULL)
        fprintf(stderr, "getenv(%s), return: %s (length: %lu)\n", env, ret, strlen(ret));
    else
        fprintf(stderr, "getenv(%s), return: NULL\n", env);
    return ret;
#else
    return getenv(env);
#endif
}

int ocall_sgx_getsockname(int s, struct sockaddr *name, unsigned nlen, socklen_t *addrlen)
{
    return getsockname((SOCKET)s, name, addrlen);
}

int ocall_sgx_getsockopt(int s, int level, int optname, void *optval, unsigned len,
        socklen_t* optlen)
{
#ifdef OCALL_TRACE
    int ret;
    ret = getsockopt((SOCKET)s, level, optname, optval, &len);
    fprintf(stderr, "getsockopt(%d, %d, %d %p, %lu), return: %d (%d)\n", s, level, optname, optval, len, ret, *optlen);
    return ret;
#else
    int ret = getsockopt((SOCKET)s, level, optname, optval, &len);
    *optlen = len;
    return ret;
#endif
}

struct servent *ocall_sgx_getservbyname(const char *name, const char *proto)
{
    // TODO: copy res to enclave mem region
    return getservbyname(name, proto);
}

struct protoent *ocall_sgx_getprotobynumber(int proto)
{
    return getprotobynumber(proto);
}

int ocall_sgx_setsockopt(int s, int level, int optname, const void *optval, size_t optlen)
{
#ifdef OCALL_TRACE
    int ret;
    ret = setsockopt(s, level, optname, optval, optlen);
    fprintf(stderr, "setsockopt(%d, %d, %d, %x, %d), return: %d\n", s, level, optname, optval, optlen, ret);
    return ret;
#else
    return setsockopt(s, level, optname, optval, optlen);
#endif
}

#include <arpa/inet.h>

unsigned short ocall_sgx_htons(unsigned short hostshort)
{
    return htons(hostshort);
}

unsigned long ocall_sgx_htonl(unsigned long hostlong)
{
    return htonl(hostlong);
}

unsigned short ocall_sgx_ntohs(unsigned short netshort)
{
    return ntohs(netshort);
}

unsigned long ocall_sgx_ntohl(unsigned long netlong)
{
    return ntohl(netlong);
}

void sgx_signal_handle_caller(int signum)
{
    fprintf(stderr, "Error: signum = %d\n", signum);
    if (signum == 2) {
        exit(0);
    }
}

#include <signal.h>
sighandler_t ocall_sgx_signal(int signum, sighandler_t a)
{
    return signal(signum, sgx_signal_handle_caller);
}

int ocall_sgx_shutdown(int sockfd, int how)
{
#ifdef OCALL_TRACE
    int ret = shutdown(sockfd, how);
    fprintf(stderr, "shutdown(%d, %d), return: %d\n", sockfd, how, ret);
    return ret;
#else
    return shutdown(sockfd, how);
#endif
}

int ocall_sgx_poll(struct pollfd *fds, size_t size, nfds_t nfds, int timeout)
{
    //fprintf(stderr, "poll...\n");
#ifdef OCALL_TRACE
    int ret = poll(fds, nfds, timeout);
    int i;
    fprintf(stderr, "poll(");
    for (i = 0; i < nfds; i++) {
        fprintf(stderr, "{fd: %d, events: %d, revents: %d}", fds[i].fd, fds[i].events, fds[i].revents);
    }
    fprintf(stderr, "%d, %d), return: %d\n", nfds, timeout, ret);
    return ret;
#else
    return poll(fds, nfds, timeout);
#endif
}

//--------------- file I/O --------------

FILE* ocall_sgx_fopen(const char *filename, const char *mode) {
  return fopen(filename, mode);
}

FILE* ocall_sgx_fdopen(int fd, const char *mode) {
  return fdopen(fd, mode);
}

int ocall_sgx_fclose(FILE *stream) {
  return fclose(stream);
}

int ocall_sgx_fseek(FILE *file, long offset, int origin) {
  return fseek(file, offset, origin);
}

size_t ocall_sgx_fwrite(const void *buffer, size_t size, size_t count, FILE* stream) {
#ifdef OCALL_TRACE
    size_t ret = fwrite(buffer, size, count, stream);
    fprintf(stderr, "fwrite(%s, %lu, %lu, 0x%p, %lu), return: %d\n", buffer, size, count, stream, ret);
    return ret;
#else
    return fwrite(buffer, size, count, stream);
#endif
}

size_t ocall_sgx_fread(void *buffer, size_t size, size_t count, FILE* stream) {
  return fread(buffer, size, count, stream);
}

int ocall_sgx_open64(const char *pathname, int flags, unsigned mode)
{
#ifdef OCALL_TRACE
    int ret;
    ret = open64(pathname, flags, mode);
    fprintf(stderr, "open64(%s, %d, %d), return: %d\n", pathname, flags, mode, ret);
    return ret;
#else
    return open64(pathname, flags, mode);
#endif
}

off_t ocall_sgx_lseek64(int fildes, off_t offset, int whence)
{
    return lseek64(fildes, offset, whence);
}

int ocall_sgx_fsync(int fd)
{
    return fsync(fd);
}

int ocall_sgx_unlink(const char *pathname) {
    return unlink(pathname);
}

int ocall_sgx_dup(int oldfd)
{
#ifdef OCALL_TRACE
    int ret;
    ret = dup(oldfd);
    fprintf(stderr, "dup(%d), return: %d\n", oldfd, ret);
    return ret;
#else
    return dup(oldfd);
#endif
}

int ocall_sgx_dup2(int oldfd, int newfd)
{
#ifdef OCALL_TRACE
    int ret;
    ret = dup2(oldfd, newfd);
    fprintf(stderr, "dup2(%d, %d), return: %d\n", oldfd, newfd, ret);
    return ret;
#else
    return dup2(oldfd, newfd);
#endif
}

int ocall_sgx_fprintf(FILE *stream, const char *buf) {
    return 0;
}

int ocall_sgx_getrlimit(int resource, struct rlimit *rlim) {
#ifdef OCALL_TRACE
    int ret = getrlimit(resource, rlim);
    fprintf(stderr, "getrlimit(%d, 0x%x), return: %d\n", resource, rlim, ret);
    return ret;
#else
    return getrlimit(resource, rlim);
#endif
}

size_t ocall_sgx_fwritex(const unsigned char *s, size_t l, FILE *stream) {
#ifdef OCALL_TRACE
    size_t ret = fwrite(s, l, 1, stream);
    fprintf(stderr, "fwritex(%s, %lu, %p), return: %lu\n", s, l, stream, ret);
    return ret;
#else
    return fwrite(s, l, 1, stream);
#endif
}

char* ocall_sgx_inet_ntop(int af, const void *src, char *dst, socklen_t size) {
#ifdef OCALL_TRACE
    char *ret = (char *)inet_ntop(af, src, dst, size);
    fprintf(stderr, "inet_ntop(%d, %p, %s, %d), return: %s\n", af, src, dst, size, ret);
    return ret;
#else
    return (char *)inet_ntop(af, src, dst, size);
#endif
}

char* ocall_sgx_inet_ntoa(struct in_addr in) {
#ifdef OCALL_TRACE
    char *ret = inet_ntoa(in);
    fprintf(stderr, "inet_ntoa(%d), return: %s\n", in.s_addr, ret);
    return ret;
#else
    return inet_ntoa(in);
#endif
}

unsigned int ocall_sgx_inet_addr(const char *cp) {
#ifdef OCALL_TRACE
    unsigned int ret = inet_addr(cp);
    fprintf(stderr, "inet_addr(%s), return: %d\n", cp, ret);
    return ret;
#else
    return inet_addr(cp);
#endif
}

int ocall_sgx_inet_pton(int af, const char *src, void *dst) {
#ifdef OCALL_TRACE
    int ret = inet_pton(af, src, dst);
    fprintf(stderr, "inet_pton(%d, %s, %p), return: %d\n", af, src, dst, ret);
    return ret;
#else
    return inet_pton(af, src, dst);
#endif
}

size_t ocall_sgx_strftime(char *s, size_t max, const char *format, const struct tm *tm) {
#ifdef OCALL_TRACE
    size_t ret = strftime(s, max, format, tm);
    fprintf(stderr, "strftime(%s, %lu, %s, %p), return: %d\n", s, max, format, tm, ret);
    return ret;
#else
    return strftime(s, max, format, tm);
#endif
}

ssize_t ocall_sgx_sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
#ifdef OCALL_TRACE
    ssize_t ret = sendfile(out_fd, in_fd, offset, count);
    if (offset == 0) {
        fprintf(stderr, "sendfile(%d, %d, NULL, %lu), return: %ld\n", out_fd, in_fd, count, ret);
    } else {
        fprintf(stderr, "sendfile(%d, %d, %d, %lu), return: %ld\n", out_fd, in_fd, *offset, count, ret);
    }
    return ret;
#else
    return sendfile(out_fd, in_fd, offset, count);
#endif
}

int ocall_sgx_geterrno() {
#ifdef OCALL_TRACE
    fprintf(stderr, "geterrno(), return: %d\n", errno);
    return errno;
#else
    return errno;
#endif
}

unsigned long ocall_sgx_rdtsc() {
    unsigned long lo, hi;
    asm volatile( "rdtsc" : "=a" (lo), "=d" (hi) );
    return( lo | ( hi << 32 ) );
}

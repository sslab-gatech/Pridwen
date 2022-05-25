#include <stdio.h>      /* vsnprintf */
#include <errno.h>
#include <string.h>

#include "sgx_trts.h"

#include "ocall_stub.h"
#include "Enclave_t.h"

#ifdef LD_DEBUG

#define abort() exit(1)
#else
#define abort()
#endif

int rand() {
   sgx_status_t sgx_retv;
   int retv;

   if ((sgx_retv = sgx_read_rand((unsigned char *)&retv, sizeof(retv)) != SGX_SUCCESS)) {
      printf(" FAILED!, Error code = %d\n", sgx_retv);
      abort();
   }

   return retv;
}

int printf(const char *fmt, ...) {
    char buf[256] = {'\0'};
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    ocall_print_string(buf);

    return 0;
}

clock_t clock() {
    clock_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_clock(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

time_t time(time_t *timep) {
    time_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_time(&retv, timep)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    size_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_strftime(&retv, s, max, format, tm)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

struct tm lt;
struct tm *localtime(const time_t *timep) {
    // TODO: copy res to enclave mem region
    struct tm *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_localtime(&retv, timep)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    memcpy((char *)&lt, (char *)retv, sizeof(struct tm));
    return &lt;
}

struct tm gt;
struct tm *gmtime(const time_t *timep) {
    // TODO: copy res to enclave mem region
    struct tm *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_gmtime(&retv, timep)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    memcpy((char *)&gt, (char *)retv, sizeof(struct tm));
    return &gt;
}

time_t mktime(struct tm *timeptr) {
    time_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_mktime(&retv, timeptr)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_gettimeofday(&retv, tv, tz)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int putchar(int c) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_putchar(&retv, c)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int puts(const char *str) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_puts(&retv, str)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int open(const char *pathname, int flags, ...) {
    int retv;
    unsigned mode;
    va_list ap;

    va_start(ap, flags);
    mode = va_arg(ap, unsigned);
    va_end;

    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_open(&retv, pathname, flags, mode)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

int close(int fd) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_close(&retv, fd)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t read(int fd, char * buf, size_t buf_len) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_read(&retv, fd, buf, buf_len)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

ssize_t write(int fd, const char *buf, size_t n) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_write(&retv, fd, buf, n)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

off_t lseek(int fildes, off_t offset, int whence) {
    off_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_lseek(&retv, fildes, offset, whence)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int socket(int af, int type, int protocol) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_socket(&retv, af, type, protocol)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

int bind(int s, const struct sockaddr *addr, size_t addr_size) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_bind(&retv, s, addr, addr_size)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int connect(int s, const struct sockaddr *addr, size_t addrlen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_connect(&retv, s, addr, addrlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

int listen(int s, int backlog) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_listen(&retv, s, backlog)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_accept(&retv, s, addr, (size_t)*addrlen, addrlen))
            != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    return retv;
}

int accept4(int s, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_accept4(&retv, s, addr, (size_t)*addrlen, addrlen, flags))
            != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

int fstat(int fd, struct stat *buf) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fstat(&retv, fd, buf)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t send(int s, const void *buf, size_t len, int flags) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_send(&retv, s, buf, len, flags)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t recv(int s, void *buf, size_t len, int flags) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_recv(&retv, s, buf, len, flags)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, size_t addrlen) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_sendto(&retv, sockfd, buf, len, flags,
                    dest_addr, addrlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t recvfrom(int s, void *buf, size_t len, int flags,
        struct sockaddr *dest_addr, socklen_t* addrlen) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_recvfrom(&retv, s, buf, len, flags,
                    dest_addr, *addrlen, addrlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    ssize_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_sendfile(&retv, out_fd, in_fd, offset, count)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    return retv;
}

int gethostname(char *name, size_t namelen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_gethostname(&retv, name, namelen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

/*
int getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints,
        struct addrinfo **res);
        */
#define INFO_MAX 64
addrinfo addrinfoarr[INFO_MAX];
int getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints, unsigned long *res) {
    // TODO: copy res to enclave mem region
    int retv, i;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getaddrinfo(&retv, node, service, hints,
                    res)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    i = 0;
    for (addrinfo *rp = (addrinfo *)*res; i < INFO_MAX && rp != NULL;
            rp = rp->ai_next, ++i)
        memcpy((char *)&addrinfoarr[i], (char *)rp, sizeof(addrinfo));
    if (i == INFO_MAX) {
        ocall_sgx_puts(&retv, "FAILED!, increase INFO_MAX!");
        abort();
    }
    return retv;
}

char envret[0x100];
char *getenv(const char *env) {
    // TODO: copy res to enclave mem region
    char *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getenv(&retv, env)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == NULL) {
        return NULL;
    }

    size_t i;
    for (i = 0;retv[i];++i) envret[i] = retv[i];
    envret[i] = '\0';
    return envret;
}

int getsockname(int s, struct sockaddr *name, socklen_t *addrlen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getsockname(&retv, s, name, (size_t)*addrlen,
                    addrlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int getsockopt(int s, int level, int optname, void *optval,
               socklen_t* optlen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getsockopt(&retv, s, level, optname, optval,
                    *optlen, optlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

struct servent serventret;
struct servent *getservbyname(const char *name, const char *proto) {
    // TODO: copy res to enclave mem region
    struct servent *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getservbyname(&retv, name, proto)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    memcpy((char *)&serventret, (char *)retv, sizeof(struct servent));
    return &serventret;
}

struct protoent protoentret;
struct protoent *getprotobynumber(int proto) {
    // TODO: copy res to enclave mem region
    struct protoent *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getprotobynumber(&retv, proto)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    memcpy((char *)&protoentret, (char *)retv, sizeof(struct protoent));
    return &protoentret;
}

int setsockopt(int s, int level, int optname, const void *optval, size_t optlen) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_setsockopt(&retv, s, level, optname, optval, optlen)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

unsigned short htons(unsigned short hostshort) {
    unsigned short retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_htons(&retv, hostshort)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

unsigned long htonl(unsigned long hostlong) {
    unsigned long retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_htonl(&retv, hostlong)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

unsigned short ntohs(unsigned short netshort) {
    unsigned short retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_ntohs(&retv, netshort)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

unsigned long ntohl(unsigned long netlong) {
    unsigned long retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_ntohl(&retv, netlong)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

sighandler_t signal(int signum, sighandler_t a) {
    sighandler_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_signal(&retv, signum, a)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int shutdown(int a, int b) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_shutdown(&retv, a, b)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    int retv;
    sgx_status_t sgx_retv;
    size_t fds_size;

    fds_size = sizeof(struct pollfd) * nfds;
    if((sgx_retv = ocall_sgx_poll(&retv, fds, fds_size, nfds, timeout)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
    }

    return retv;
}

FILE* fopen(const char *filename, const char *mode) {
    FILE *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fopen(&retv, filename, mode)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

FILE* fdopen(int fd, const char *mode) {
    FILE *retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fdopen(&retv, fd, mode)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int fclose(FILE *stream) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fclose(&retv, stream)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int fseek(FILE *file, long offset, int origin) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fseek(&retv, file, offset, origin)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream) {
    size_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fwrite(&retv, buffer, size, count, stream)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}


size_t fread(void *buffer, size_t size, size_t count, FILE *stream) {
    size_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fread(&retv, buffer, size, count, stream)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int fprintf(FILE *stream, const char *fmt, ...) {
#if 0
    char buf[256] = {'\0'};
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    printf("fprintf  ");
    if (stream == (FILE *)stderr) {
        printf("stream = stderr");
    } else if (stream == (FILE *)stdout) {
        printf("stream = stdout");
    } else {
        printf("stream = file");
    }
    printf("\n");

    //ocall_print_string(buf);
#endif
    return 0;
}

void exit(int status) {
    printf("exit\n");
}

int ferror(FILE *stream) {
    printf("ferror\n");
    return 0;
}

off_t lseek64(int fildes, off_t offset, int whence) {
    off_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_lseek64(&retv, fildes, offset, whence)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}


int open64(const char *pathname, int flags, unsigned mode) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_open64(&retv, pathname, flags, mode)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int access(const char *pathname, int mode) {
    printf("sgx_access\n");
    return 0;
}

char* getcwd(char *buf, size_t size) {
    sgx_status_t sgx_retv;
    char *retv;

    retv = (char *)malloc(size);
    if((sgx_retv = ocall_sgx_getcwd(&retv, buf, size)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    memcpy((char *)retv, (char *)buf, size);

    //printf("getcwd: %s\n", buf);

    return buf;
}

int stat(const char *pathname, struct stat *statbuf) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_stat(&retv, pathname, statbuf)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    return retv;
}

int ftruncate(int fd, off_t length) {
    printf("sgx_ftruncate\n");
    return 0;
}

int ftruncate64(int fd, off_t length) {
    printf("sgx_ftruncate64\n");
    return 0;
}

int fcntl(int fd, int cmd, ...) {
    int retv;
    sgx_status_t sgx_retv;
    va_list ap;
    void *ptr;
    void *arg;
    size_t size;

    va_start(ap, cmd);
    arg = va_arg(ap, void *);
    va_end(ap);

    switch (cmd) {
    case F_SETFD:
         //printf("cmd: F_SETFD\n");
         size = sizeof(int);
         ptr = &arg;
         break;
    case F_GETFD:
         //printf("cmd: F_GETFD\n");
         ptr = &arg;
         size = 0;
         break;
    case F_SETFL:
         size = sizeof(int);
         ptr = &arg;
         break;
    case F_GETFL:
         size = sizeof(int);
         ptr = &arg;
         break;
    case F_SETLK:
    case F_GETLK:
         //printf("cmd: F_SETLK/F_GETLK\n");
         size = sizeof(struct flock);
         ptr = arg;
         break;
    default:
      printf("Unsupported cmd: %d\n", cmd);
    }

    if((sgx_retv = ocall_sgx_fcntl(&retv, fd, cmd, ptr, size)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int fchmod(int fd, mode_t mode) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fchmod(&retv, fd, mode)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int unlink(const char *pathname) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_unlink(&retv, pathname)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int mkdir(int dirfd, const char *pathname, mode_t mode) {
    printf("sgx_mkdir\n");
    return 0;
}

int rmdir(const char *pathname) {
    printf("sgx_rmdir\n");
    return 0;
}

uid_t geteuid(void) {
    uid_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_geteuid(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int ioctl(int fd, unsigned long request, ...) {
    int retv;
    void *arg;
    va_list ap;
    size_t size;

    va_start(ap, request);
    arg = va_arg(ap, void *);
    va_end(ap);

    size = sizeof(int);

    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_ioctl(&retv, fd, request, arg, size)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    return retv;
}

pid_t getpid() {
    pid_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getpid(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

uid_t getuid() {
    uid_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getuid(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

gid_t getgid() {
    gid_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getgid(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

gid_t getegid() {
    gid_t retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getegid(&retv)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int fsync(int fd) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_fsync(&retv, fd)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int dup(int oldfd) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_dup(&retv, oldfd)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int dup2(int oldfd, int newfd) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_dup2(&retv, oldfd, newfd)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int getrlimit(int resource, struct rlimit *rlim) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_getrlimit(&retv, resource, rlim)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == -1) {
        if ((sgx_retv = ocall_sgx_geterrno(&errno)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
        printf("getrlimit failed! errno (%p): %d\n", &errno, errno);
    }

    return retv;
}

char *inet_ntop_ret;
char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    char *retv;
    size_t len;
    sgx_status_t sgx_retv;

    retv = (char *)malloc(size);
    if((sgx_retv = ocall_sgx_inet_ntop(&retv, af, src, dst, size)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == NULL) {
        return NULL;
    }

    inet_ntop_ret = (char *)malloc(size);
    memcpy(retv, inet_ntop_ret, size);

    return inet_ntop_ret;
}

char inet_ntoa_ret[200];
char *inet_ntoa(struct in_addr in) {
    char *retv;
    size_t len;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_inet_ntoa(&retv, in)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }

    if (retv == NULL) {
        return NULL;
    }

    size_t i;
    for (i = 0;retv[i];++i) inet_ntoa_ret[i] = retv[i];
    inet_ntoa_ret[i] = '\0';

    return inet_ntoa_ret;
}

unsigned int inet_addr(const char *cp) {
    unsigned int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_inet_addr(&retv, cp)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int inet_pton(int ad, const char *src, void *dst) {
    int retv;
    sgx_status_t sgx_retv;
    if((sgx_retv = ocall_sgx_inet_pton(&retv, ad, src, dst)) != SGX_SUCCESS) {
        printf(" FAILED!, Error code = %d\n", sgx_retv);
        abort();
    }
    return retv;
}

int mmap() {
    printf("sgx_mmap\n");
    return 0;
}

int munmap() {
    printf("sgx_munmap\n");
    return 0;
}

int syscall() {
    printf("sgx_syscall\n");
    return 0;
}

int clone() {
    printf("sgx_clone\n");
    return 0;
}

int clock_gettime() {
    printf("sgx_clock_gettime\n");
    return 0;
}

int mremap() {
    printf("sgx_mremap\n");
    return 0;
}

// sys/uio.h
ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t retv;
    ssize_t n;
    sgx_status_t sgx_retv;
    int i;

    retv = 0;
    for (i = 0; i < iovcnt; i++) {
        void *buf = iov[i].iov_base;
	size_t len = iov[i].iov_len;
        if((sgx_retv = ocall_sgx_write(&n, fd, buf, len)) != SGX_SUCCESS) {
            printf(" FAILED!, Error code = %d\n", sgx_retv);
            abort();
        }
        retv += n;
    }
    return retv;
}

// sys/socekt.h
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return 0;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return 1;
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return 1;
}

// unistd.h
int chdir(const char *path) {
    return 0;
}

int pipe(int pipefd[2]) {
    return 0;
}

#define COROUTINE_HOOK
#ifdef COROUTINE_HOOK

#include "../coroutine/coroutine.h"
#include <limits.h>

typedef int (*socket_t)(int domain, int type, int protocol);
typedef int(*connect_t)(int, const struct sockaddr *, socklen_t);
typedef ssize_t(*read_t)(int, void *, size_t);
typedef ssize_t(*recv_t)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t(*recvfrom_t)(int sockfd, void *buf, size_t len, int flags,
        struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t(*write_t)(int, const void *, size_t);
typedef ssize_t(*send_t)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t(*sendto_t)(int sockfd, const void *buf, size_t len, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen);
typedef int(*accept_t)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
typedef int(*close_t)(int);

extern socket_t socket_f;
extern connect_t connect_f;
extern read_t read_f;
extern recv_t recv_f;
extern recvfrom_t recvfrom_f;
extern write_t write_f;
extern send_t send_f;
extern sendto_t sendto_f;
extern accept_t accept_f;
extern close_t close_f;


int init_hook(void);


#endif // 
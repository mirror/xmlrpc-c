#ifndef SOCKUTIL_H_INCLUDED
#define SOCKUTIL_H_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>

typedef struct {
    int interruptorFd;
    int interrupteeFd;
} sockutil_InterruptPipe;

void
sockutil_interruptPipeInit(sockutil_InterruptPipe * const pipeP,
                           const char **            const errorP);

void
sockutil_interruptPipeTerm(sockutil_InterruptPipe const pipe);

void
sockutil_interruptPipeInterrupt(sockutil_InterruptPipe const interruptPipe);

bool
sockutil_connected(int const fd);

void
sockutil_getSockName(int                const sockFd,
                     struct sockaddr ** const sockaddrPP,
                     size_t  *          const sockaddrLenP,
                     const char **      const errorP);

void
sockutil_getPeerName(int                const sockFd,
                     struct sockaddr ** const sockaddrPP,
                     size_t  *          const sockaddrLenP,
                     const char **      const errorP);

void
sockutil_formatPeerInfo(int           const sockFd,
                        const char ** const peerStringP);

void
sockutil_listen(int          const sockFd,
                uint32_t      const backlog,
                const char ** const errorP);

void
sockutil_waitForConnection(int                    const listenSockFd,
                           sockutil_InterruptPipe const interruptPipe,
                           bool *                 const interruptedP,
                           const char **          const errorP);

void
sockutil_setSocketOptions(int           const fd,
                          const char ** const errorP);

void
sockutil_bindSocketToPort(int                     const fd,
                          const struct sockaddr * const sockAddrP,
                          socklen_t               const sockAddrLen,
                          const char **           const errorP);

void
sockutil_bindSocketToPortInet(int           const fd,
                              uint16_t      const portNumber,
                              const char ** const errorP);

void
sockutil_bindSocketToPortInet6(int                     const fd,
                               uint16_t                const portNumber,
                               const char **           const errorP);

#endif

#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

#include <netinet/in.h>

#include "xmlrpc-c/abyss.h"

/* Eventually, TSocket will be a struct, will be exposed to the user,
   and will be able to represent a much broader range of sockets. 
*/
typedef TOsSocket TSocket;

#define TIME_INFINITE   0xffffffff

#define IPB1(x) (((unsigned char *)(&x))[0])
#define IPB2(x) (((unsigned char *)(&x))[1])
#define IPB3(x) (((unsigned char *)(&x))[2])
#define IPB4(x) (((unsigned char *)(&x))[3])

typedef struct in_addr TIPAddr;

abyss_bool SocketInit(void);

void
SocketCreate(TSocket ** const socketPP);

void
SocketDestroy(TSocket * const socketP);

void
SocketWrite(TSocket *             const socketP,
            const unsigned char * const buffer,
            uint32_t              const len,
            abyss_bool *          const failedP);

uint32_t SocketRead(TSocket *s, char *buffer, uint32_t len);
uint32_t SocketPeek(TSocket *s, char *buffer, uint32_t len);

abyss_bool SocketConnect(TSocket *s, TIPAddr *addr, uint16_t port);
abyss_bool SocketBind(TSocket *s, TIPAddr *addr, uint16_t port);

abyss_bool
SocketListen(const TSocket * const socketFdP,
             uint32_t        const backlog);

void
SocketAccept(TSocket *    const listenSocketP,
             abyss_bool * const connectedP,
             abyss_bool * const failedP,
             TSocket **   const acceptedSocketPP,
             TIPAddr *    const ipAddr);

uint32_t SocketError(void);

uint32_t SocketWait(TSocket *s,abyss_bool rd,abyss_bool wr,uint32_t timems);

abyss_bool SocketBlocking(TSocket *s, abyss_bool b);
uint32_t SocketAvailableReadBytes(TSocket *s);

void
SocketGetPeerName(TSocket *    const socketP,
                  TIPAddr *    const ipAddrP,
                  uint16_t *   const portNumberP,
                  abyss_bool * const successP);

#endif

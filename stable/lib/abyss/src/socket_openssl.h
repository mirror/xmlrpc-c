#ifndef SOCKET_OPENSSL_H_INCLUDED
#define SOCKET_OPENSSL_H_INCLUDED

#include <sys/socket.h>

#include <xmlrpc-c/abyss.h>

void
SocketOpenSslInit(const char ** const errorP);

void
SocketOpenSslTerm(void);

#endif

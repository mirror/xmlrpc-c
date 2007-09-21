#ifndef CONN_H_INCLUDED
#define CONN_H_INCLUDED

#include "xmlrpc-c/abyss.h"
#include "socket.h"
#include "file.h"

#define BUFFER_SIZE 4096 

struct _TConn {
    TServer * server;
    uint32_t buffersize;
        /* Index into the connection buffer (buffer[], below) where
           the next byte read on the connection will go.
        */
    uint32_t bufferpos;
        /* Index into the connection buffer (buffer[], below) where
           the next byte to be delivered to the user is.
        */
    uint32_t inbytes,outbytes;  
    TSocket socket;
    TIPAddr peerip;
    abyss_bool hasOwnThread;
    TThread thread;
    abyss_bool connected;
    abyss_bool inUse;
    const char * trace;
    void (*job)(struct _TConn *);
    char buffer[BUFFER_SIZE];
};

typedef struct _TConn TConn;

TConn * ConnAlloc(void);

void ConnFree(TConn * const connectionP);

abyss_bool
ConnCreate(TConn *   const connectionP,
           TSocket * const s,
           void (*         func)(TConn *));

abyss_bool ConnCreate2(TConn *             const connectionP, 
                       TServer *           const serverP,
                       TSocket             const connectedSocket,
                       void            ( *       func)(TConn *),
                       enum abyss_foreback const foregroundBackground);

abyss_bool
ConnProcess(TConn * const connectionP);

abyss_bool
ConnKill(TConn * const connectionP);

void
ConnClose(TConn * const connectionP);

abyss_bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size);

abyss_bool
ConnRead(TConn *  const c,
         uint32_t const timems);

void
ConnReadInit(TConn * const connectionP);

abyss_bool
ConnReadHeader(TConn * const connectionP,
               char ** const headerP);

abyss_bool
ConnWriteFromFile(TConn *  const connectionP,
                  TFile *  const file,
                  uint64_t const start,
                  uint64_t const end,
                  void *   const buffer,
                  uint32_t const buffersize,
                  uint32_t const rate);

TServer *
ConnServer(TConn * const connectionP);

#endif

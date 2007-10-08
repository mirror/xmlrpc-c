#ifndef CHANNEL_H_INCLUDED
#define CHANNEL_H_INCLUDED

/*============================================================================
   This is the generic channel interface for Abyss.  It includes both
   the generic interface to a channel from above and the interface
   between generic channel code and a particular channel
   implementation (e.g. POSIX socket) below.

   Abyss uses a channel to converse with an HTTP client.  A channel is
   oblivious to HTTP -- it just transports a byte stream in each direction.
============================================================================*/

#include "int.h"
#include "xmlrpc-c/abyss.h"

struct TChannelVtbl;

void
ChannelCreate(const struct TChannelVtbl * const vtblP,
              void *                      const implP,
              TChannel **                 const channelPP);

typedef void ChannelDestroyImpl(TChannel * const channelP);

typedef void ChannelWriteImpl(TChannel *             const channelP,
                              const unsigned char *  const buffer,
                              uint32_t               const len,
                              abyss_bool *           const failedP);

typedef uint32_t ChannelReadImpl(TChannel * const channelP,
                                 char *    const buffer,
                                 uint32_t  const len);

typedef uint32_t ChannelErrorImpl(TChannel * const channelP);

typedef uint32_t ChannelWaitImpl(TChannel * const channelP,
                                 abyss_bool const rd,
                                 abyss_bool const wr,
                                 uint32_t   const timems);

typedef uint32_t ChannelAvailableReadBytesImpl(TChannel * const channelP);

typedef void ChannelFormatPeerInfoImpl(TChannel *    const channelP,
                                       const char ** const peerStringP);

struct TChannelVtbl {
    ChannelDestroyImpl            * destroy;
    ChannelWriteImpl              * write;
    ChannelReadImpl               * read;
    ChannelWaitImpl               * wait;
    ChannelAvailableReadBytesImpl * availableReadBytes;
    ChannelFormatPeerInfoImpl     * formatPeerInfo;
};

struct _TChannel {
    uint                signature;
        /* With both background and foreground use of sockets, and
           background being both fork and pthread, it is very easy to
           screw up socket lifetime and try to destroy twice.  We use
           this signature to help catch such bugs.
        */
    void *              implP;
    struct TChannelVtbl vtbl;
};

#define TIME_INFINITE   0xffffffff

extern abyss_bool ChannelTraceIsActive;

void
ChannelInit(const char ** const errorP);

void
ChannelTerm(void);

void
ChannelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             abyss_bool *          const failedP);

uint32_t
ChannelRead(TChannel *      const channelP, 
            unsigned char * const buffer, 
            uint32_t        const len);

uint32_t
ChannelWait(TChannel * const channelP,
            abyss_bool const rd,
            abyss_bool const wr,
            uint32_t   const timems);

uint32_t
ChannelAvailableReadBytes(TChannel * const channelP);

void
ChannelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP);

#endif

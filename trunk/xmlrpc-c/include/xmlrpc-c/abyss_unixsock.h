/* This is just a sub-file for abyss.h */

struct sockaddr;

void
ChanSwitchUnixCreate(uint16_t       const portNumber,
                     TChanSwitch ** const chanSwitchPP,
                     const char **  const errorP);

void
ChanSwitchUnixCreateFd(int            const fd,
                       TChanSwitch ** const chanSwitchPP,
                       const char **  const errorP);

void
ChannelUnixCreateFd(int           const fd,
                    TChannel **   const channelPP,
                    const char ** const errorP);

void
ChannelUnixGetPeerName(TChannel *         const channelP,
                       struct sockaddr ** const sockaddrPP,
                       size_t  *          const sockaddrLenP,
                       const char **      const errorP);

void
SocketUnixCreateFd(int        const fd,
                   TSocket ** const socketPP);

typedef int TOsSocket;

/* This is just a sub-file for abyss.h */

#include <winsock.h>

void
ChanSwitchWinCreate(unsigned short const portNumber,
                    TChanSwitch ** const chanSwitchPP,
                    const char **  const errorP);

void
ChanSwitchWinCreateWinsock(SOCKET         const winsock,
                           TChanSwitch ** const chanSwitchPP,
                           const char **  const errorP);

void
ChannelWinCreate(TChanSwitch ** const chanSwitchPP);

void
ChannelWinCreateWinsock(SOCKET        const winsock,
                        TChannel **   const channelPP,
                        void **       const channelInfoPP,
                        const char ** const errorP);

void
SocketWinCreate(TSocket ** const socketPP);

void
SocketWinCreateWinsock(SOCKET     const winsock,
                       TSocket ** const socketPP);

typedef SOCKET TOsSocket;

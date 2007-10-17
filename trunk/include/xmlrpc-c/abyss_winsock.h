/* This is just a sub-file for abyss.h */

#include <winsock.h>

struct abyss_win_chaninfo {
    size_t peerAddrLen;
    struct sockaddr peerAddr;
};


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
ChannelWinCreateWinsock(SOCKET                       const fd,
                        TChannel **                  const channelPP,
                        struct abyss_win_chaninfo ** const channelInfoPP,
                        const char **                const errorP);

void
SocketWinCreate(TSocket ** const socketPP);

void
SocketWinCreateWinsock(SOCKET     const winsock,
                       TSocket ** const socketPP);

typedef SOCKET TOsSocket;

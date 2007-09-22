/* This is just a sub-file for abyss.h */

void
ChanSwitchWinCreate(TChanSwitch ** const chanSwitchPP);

void
ChanSwitchWinCreateWinsock(SOCKET      const winsock,
                           TChannel ** const channelPP);

void
ChannelWinCreate(TChanSwitch ** const chanSwitchPP);

void
ChannelWinCreateWinsock(SOCKET      const winsock,
                        TChannel ** const channelPP);

void
SocketWinCreate(TSocket ** const socketPP);

void
SocketWinCreateWinsock(SOCKET     const winsock,
                       TSocket ** const socketPP);

typedef SOCKET TOsSocket;

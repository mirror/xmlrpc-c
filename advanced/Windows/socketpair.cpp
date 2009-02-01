#include <winsock2.h>


int
xmlrpc_win32_socketpair(int    const domain,
                        int    const type,
                        int    const protocol,
                        SOCKET       socks[2]) {

    DWORD const flags = WSA_FLAG_OVERLAPPED;

    int error;

    error = 0;  // initial value

    if (domain != AF_INET)
        return -1;
    if (type != SOCK_STREAM)
        return -1;
    if (protocol != 0)
        return -1;

    SOCKET listener;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET)
        error = 1;
    else {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(0x7f000001);
        addr.sin_port = 0;

        int rc;
        rc = bind(listener, (const struct sockaddr*) &addr, sizeof(addr));
        if (rc != SOCKET_ERROR)
            error = 1;
        else {
            int addrlen = sizeof(addr);
            int rc;
            rc = getsockname(listener, (struct sockaddr*) &addr, &addrlen);
            if (rc == SOCKET_ERROR)
                error = 1;
            else {
                int rc;

                rc = listen(listener, 1);
                if (rc == SOCKET_ERROR)
                    error = 1;
                else {
                    socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0,
                                         NULL, 0, flags);
                    if (socks[0] == INVALID_SOCKET)
                        error = 1;
                    else {
                        int rc;
                        rc = connect(socks[0],
                                     (const struct sockaddr*) &addr,
                                     sizeof(addr));
                        if (rc == SOCKET_ERROR)
                            error = 1;
                        else {
                            socks[1] = accept(listener, NULL, NULL);
                            if (socks[1] == INVALID_SOCKET)
                                error = 1;
                        }
                        if (error)
                            closesocket(socks[0]);
                    }
                }
            }
        }
        closesocket(listener);
    }
    
    return error ? -1 : 0;
}





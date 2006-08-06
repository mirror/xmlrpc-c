/* None of the tests in here rely on a client existing, or even a network
   connection.  We should figure out how to create a test client and do
   such tests.
*/

#include "unistdx.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/abyss.h"

#include "test.h"

#include "abyss.h"



static void
bindSocketToPort(int      const fd,
                 uint16_t const portNumber) {

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));
    if (rc != 0)
        fprintf(stderr, "bind() of %d failed, errno=%d (%s)",
                fd, errno, strerror(errno));

    TEST(rc == 0);
}



static void
testChanSwitchOsSocket(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TChanSwitch * chanSwitchP;
        TServer server;
        const char * error;

        bindSocketToPort(fd, 8080);

        ChanSwitchUnixCreateFd(fd, &chanSwitchP, &error);
        
        TEST_NULL_STRING(error);
        
        ServerCreateSwitch(&server, chanSwitchP, &error);
        
        TEST_NULL_STRING(error);
        
        ServerFree(&server);
        
        ChanSwitchDestroy(chanSwitchP);

        close(fd);
    }
}



static void
testChanSwitch(void) {

    TServer server;
    TChanSwitch * chanSwitchP;
    const char * error;

    ChanSwitchUnixCreate(8080, &chanSwitchP, &error);

    TEST_NULL_STRING(error);

    ServerCreateSwitch(&server, chanSwitchP, &error);

    TEST_NULL_STRING(error);

    ServerFree(&server);

    ChanSwitchDestroy(chanSwitchP);
    
    testChanSwitchOsSocket();
}



static void
testChannel(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TChannel * channelP;
        struct abyss_unix_chaninfo * channelInfoP;
        const char * error;

        ChannelUnixCreateFd(fd, &channelP, &channelInfoP, &error);

        TEST(strstr(error, "not in connected"));
    }
}



static void
testOsSocket(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TServer server;
        abyss_bool success;

        bindSocketToPort(fd, 8080);

        success = ServerCreateSocket(&server, NULL, fd, NULL, NULL);

        TEST(success);

        ServerFree(&server);

        close(fd);
    }
}



static void
testSocket(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TSocket * socketP;
        TServer server;
        const char * error;

        SocketUnixCreateFd(fd, &socketP);

        TEST(socketP != NULL);

        ServerCreateSocket2(&server, socketP, &error);

        TEST(!error);

        ServerFree(&server);

        SocketDestroy(socketP);

        close(fd);
    }
}



static void
testServerCreate(void) {

    TServer server;
    abyss_bool success;

    success = ServerCreate(&server, NULL, 8080, NULL, NULL);
    TEST(success);
    ServerInit(&server);
    ServerFree(&server);

    success = ServerCreate(&server, "myserver", 8080,
                           "/tmp/docroot", "/tmp/logfile");
    TEST(success);
    ServerInit(&server);
    ServerFree(&server);

    success = ServerCreateNoAccept(&server, NULL, NULL, NULL);
    TEST(success);
    ServerFree(&server);

    {
        TChanSwitch * chanSwitchP;
        const char * error;

        ChanSwitchUnixCreate(8080, &chanSwitchP, &error);

        TEST_NULL_STRING(error);

        ServerCreateSwitch(&server, chanSwitchP, &error);
        
        TEST_NULL_STRING(error);

        ServerSetName(&server, "/tmp/docroot");
        ServerSetLogFileName(&server, "/tmp/logfile");
        ServerSetKeepaliveTimeout(&server, 50);
        ServerSetKeepaliveMaxConn(&server, 5);
        ServerSetTimeout(&server, 75);
        ServerSetAdvertise(&server, 1);
        ServerSetAdvertise(&server, 0);

        ServerInit(&server);

        ServerFree(&server);

        ChanSwitchDestroy(chanSwitchP);
    }        
}



void
test_abyss(void) {

    printf("Running Abyss server tests...\n");

    testChanSwitch();

    testChannel();

    testOsSocket();

    testSocket();

    testServerCreate();

    printf("\n");
    printf("Abyss server tests done.\n");
}

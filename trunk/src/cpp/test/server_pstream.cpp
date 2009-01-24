/*=============================================================================
                                  server_pstream
===============================================================================
  Test the pstream server C++ facilities of XML-RPC for C/C++.
  
=============================================================================*/
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <fcntl.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_pstream.hpp"

#include "tools.hpp"
#include "server_pstream.hpp"

using namespace xmlrpc_c;
using namespace std;


#define ESC_STR "\x1B"


static string const
xmlPrologue("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");

static string const
packetStart(ESC_STR "PKT");

static string const
packetEnd(ESC_STR "END");


class sampleAddMethod : public method {
public:
    sampleAddMethod() {
        this->_signature = "i:ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};



class client {
/*----------------------------------------------------------------------------
   This is an object you can use as a client to test a packet stream
   server.

   You attach the 'serverFd' member to your packet stream server, then
   call the 'sendCall' method to send a call to your server, then call
   the 'recvResp' method to get the response.

   Destroying the object closes the connection.

   We rely on typical, though unguaranteed socket function: we need to
   be able to write 'contents' to the socket in a single write()
   system call before the other side reads anything -- i.e. the socket
   has to have a buffer that big.  We do this because we're lazy; doing
   it right would require forking a writer process.
-----------------------------------------------------------------------------*/
public:

    client();
    
    ~client();

    void
    sendCall(string const& callBytes) const;

    void
    hangup();

    void
    recvResp(string * const respBytesP) const;

    int serverFd;

private:

    bool closed;
    int clientFd;
};



client::client() {

    enum {
        SERVER = 0,
        CLIENT = 1,
    };
    int sockets[2];
    int rc;

    rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);

    if (rc < 0)
        throwf("Failed to create UNIX domain stream socket pair "
               "as test tool.  errno=%d (%s)",
               errno, strerror(errno));
    else {
        fcntl(sockets[CLIENT], F_SETFL, O_NONBLOCK);

        this->serverFd = sockets[SERVER];
        this->clientFd = sockets[CLIENT];
        this->closed   = false;
    }
}



client::~client() {

    if (!closed)
        close(this->clientFd);

    close(this->serverFd);
}



void
client::sendCall(string const& packetBytes) const {

    int rc;

    rc = send(this->clientFd, packetBytes.c_str(), packetBytes.length(), 0);

    if (rc < 0)
        throwf("send() of test data to socket failed, errno=%d (%s)",
               errno, strerror(errno));
    else {
        unsigned int bytesWritten(rc);

        if (bytesWritten != packetBytes.length())
            throwf("Short write to socket");
    }
}



void
client::hangup() {

    close(this->clientFd);

    this->closed = true;
}



void
client::recvResp(string * const packetBytesP) const {

    char buffer[4096];
    int rc;

    rc = recv(this->clientFd, buffer, sizeof(buffer), 0);

    if (rc < 0)
        throwf("recv() from socket failed, errno=%d (%s)",
               errno, strerror(errno));
    else {
        unsigned int bytesReceived(rc);

        *packetBytesP = string(buffer, bytesReceived);
    }
}



static void
testEmptyStream(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send the pstream server an empty stream; i.e. we close the
   socket from the client end without sending anything.

   This should cause the server to recognize EOF.
-----------------------------------------------------------------------------*/

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.hangup();

    bool eof;
    server.runOnce(&eof);

    TEST(eof);
}



static void
testBrokenPacket(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send a stream that is not a legal packetsocket stream: it
   doesn't have any control word.
-----------------------------------------------------------------------------*/
    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall("junk");
    client.hangup();

    bool eof;

    EXPECT_ERROR(
        server.runOnce(&eof);
        );
}



static void
testEmptyPacket(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send the pstream server one empty packet.  It should respond
   with one packet, being an XML-RPC fault response complaining that the
   call is not valid XML.
-----------------------------------------------------------------------------*/
    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall(packetStart + packetEnd);

    bool eof;
    server.runOnce(&eof);

    TEST(!eof);

    string response;
    client.recvResp(&response);

    // We ought to validate that the response is a complaint about
    // the empty call

    client.hangup();

    server.runOnce(&eof);

    TEST(eof);
}



static void
testNormalCall(registry const& myRegistry) {

    string const sampleAddGoodCallStream(
        packetStart +
        xmlPrologue +
        "<methodCall>\r\n"
        "<methodName>sample.add</methodName>\r\n"
        "<params>\r\n"
        "<param><value><i4>5</i4></value></param>\r\n"
        "<param><value><i4>7</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodCall>\r\n" +
        packetEnd
        );
    

    string const sampleAddGoodResponseStream(
        packetStart +
        xmlPrologue +
        "<methodResponse>\r\n"
        "<params>\r\n"
        "<param><value><i4>12</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n" +
        packetEnd
        );

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall(sampleAddGoodCallStream);

    bool eof;
    server.runOnce(&eof);

    TEST(!eof);

    string response;
    client.recvResp(&response);

    TEST(response == sampleAddGoodResponseStream);
    
    client.hangup();

    server.runOnce(&eof);

    TEST(eof);
}



class serverPstreamConnTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "serverPstreamConnTestSuite";
    }
    virtual void runtests(unsigned int const) {
        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));

        registryPtr myRegistryP(new registry);

        myRegistryP->addMethod("sample.add", methodPtr(new sampleAddMethod));

        EXPECT_ERROR(  // Empty options
            serverPstreamConn::constrOpt opt;
            serverPstreamConn server(opt);
            );

        EXPECT_ERROR(  // No registry
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .socketFd(3));
            );

        EXPECT_ERROR(  // No socket fd
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry));
            );
        
        EXPECT_ERROR(  // No such file descriptor
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry)
                                     .socketFd(37));
            );
        
        testEmptyStream(myRegistry);

        testBrokenPacket(myRegistry);

        testEmptyPacket(myRegistry);

        testNormalCall(myRegistry);
    }
};



string
serverPstreamTestSuite::suiteName() {
    return "serverPstreamTestSuite";
}


void
serverPstreamTestSuite::runtests(unsigned int const indentation) {

    serverPstreamConnTestSuite().run(indentation + 1);

}


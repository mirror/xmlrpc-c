/*=============================================================================
                              pstream
===============================================================================

   Client XML transport for Xmlrpc-c based on a very simple byte
   stream.
   
   The protocol we use is the "packet socket" protocol, which
   is an Xmlrpc-c invention.  It is an almost trivial representation of
   a sequence of packets on a byte stream.

   A transport object talks to exactly one server over its lifetime.

   You can create a pstream transport from any file descriptor from which
   you can read and write a bidirectional character stream.  Typically,
   it's a TCP socket.

   This transport is synchronous only.  It does not provide a working
   'start' method.  You have at most one outstanding RPC and wait for
   it to complete.

   By Bryan Henderson 07.05.12.

   Contributed to the public domain by its author.
=============================================================================*/

#include <memory>

using namespace std;

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;
#include "xmlrpc-c/packetsocket.hpp"

#include "xmlrpc-c/client_transport.hpp"

typedef xmlrpc_c::clientXmlTransport_pstream::BrokenConnectionEx
    BrokenConnectionEx;

namespace xmlrpc_c {

struct clientXmlTransport_pstream::constrOpt_impl {

    constrOpt_impl();

    struct {
        int         fd;
        bool        useBrokenConnEx;
    } value;
    struct {
        bool fd;
        bool useBrokenConnEx;
    } present;
};



clientXmlTransport_pstream::constrOpt_impl::constrOpt_impl() {

    this->present.fd              = false;
    this->present.useBrokenConnEx = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
clientXmlTransport_pstream::constrOpt & \
clientXmlTransport_pstream::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->implP->value.OPTION_NAME = arg; \
    this->implP->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(fd, xmlrpc_socket);
DEFINE_OPTION_SETTER(useBrokenConnEx, bool);

#undef DEFINE_OPTION_SETTER



clientXmlTransport_pstream::constrOpt::constrOpt() {

    this->implP = new clientXmlTransport_pstream::constrOpt_impl();
}



clientXmlTransport_pstream::constrOpt::~constrOpt() {

    delete(this->implP);
}



clientXmlTransport_pstream::constrOpt::constrOpt(constrOpt& arg) {

    this->implP = new clientXmlTransport_pstream::constrOpt_impl(*arg.implP);
}



class clientXmlTransport_pstream_impl {

public:
    clientXmlTransport_pstream_impl(
        clientXmlTransport_pstream::constrOpt_impl const& opt);

    ~clientXmlTransport_pstream_impl();

    void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP);

private:
    packetSocket * packetSocketP;

    bool usingBrokenConnEx;
        // We're throwing a Broken Connection object when something fails
        // because the connection to the server is broken.  When this is false,
        // We throw an ordinary error when that happens.

    void
    sendCall(std::string const& callXml);

    void
    recvResp(std::string * const responseXmlP);
};



clientXmlTransport_pstream_impl::clientXmlTransport_pstream_impl(
    clientXmlTransport_pstream::constrOpt_impl const& opt) {

    if (!opt.present.fd)
        throwf("You must provide a 'fd' constructor option.");

    auto_ptr<packetSocket> packetSocketAP;

    try {
        auto_ptr<packetSocket> p(new packetSocket(opt.value.fd));
        packetSocketAP = p;
    } catch (exception const& e) {
        throwf("Unable to create packet socket out of file descriptor %d.  %s",
               opt.value.fd, e.what());
    }

    if (opt.present.useBrokenConnEx)
        this->usingBrokenConnEx = opt.value.useBrokenConnEx;
    else
        this->usingBrokenConnEx = false;

    this->packetSocketP = packetSocketAP.release();
}



clientXmlTransport_pstream::clientXmlTransport_pstream(
    constrOpt const& optExt) :

    implP(new clientXmlTransport_pstream_impl(*optExt.implP))
{}



clientXmlTransport_pstream_impl::~clientXmlTransport_pstream_impl() {

    delete(this->packetSocketP);
}



clientXmlTransport_pstream::~clientXmlTransport_pstream() {

    delete(this->implP);
}



void  // private
clientXmlTransport_pstream_impl::sendCall(string const& callXml) {
/*----------------------------------------------------------------------------
   Send the text 'callXml' down the pipe as a packet which is the RPC call.
-----------------------------------------------------------------------------*/
    packetPtr const callPacketP(new packet(callXml.c_str(), callXml.length()));

    try {
        bool brokenConn;

        this->packetSocketP->writeWait(callPacketP, &brokenConn);

        if (brokenConn) {
            if (this->usingBrokenConnEx)
                throw BrokenConnectionEx();
            else
                throwf("Server hung up or connection broke");
        }
    } catch (exception const& e) {
        throwf("Failed to write the call to the packet socket.  %s", e.what());
    }
}



void
clientXmlTransport_pstream_impl::recvResp(string * const responseXmlP) {
/*----------------------------------------------------------------------------
   Receive a packet which is the RPC response and return its contents
   as the text *responseXmlP.
-----------------------------------------------------------------------------*/
    packetPtr responsePacketP;

    try {
        bool eof;
        this->packetSocketP->readWait(&eof, &responsePacketP);

        if (eof) {
            if (this->usingBrokenConnEx)
                throw BrokenConnectionEx();
            else
                throwf("The other end closed the socket before sending "
                       "the response.");
        }
    } catch (exception const& e) {
        throwf("We sent the call, but couldn't get the response.  %s",
               e.what());
    }
    *responseXmlP = 
        string(reinterpret_cast<char *>(responsePacketP->getBytes()),
               responsePacketP->getLength());
}



void
clientXmlTransport_pstream_impl::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    carriageParm_pstream * const carriageParmPstreamP(
        dynamic_cast<carriageParm_pstream *>(carriageParmP));

    if (carriageParmPstreamP == NULL)
        throwf("Pstream client XML transport called with carriage "
               "parameter object not of class carriageParm_pstream");

    this->sendCall(callXml);

    this->recvResp(responseXmlP);
}



void
clientXmlTransport_pstream::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    this->implP->call(carriageParmP, callXml, responseXmlP);
}

} // namespace

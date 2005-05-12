#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <string>

#include <xmlrpc-c/base.hpp>

namespace xmlrpc_c {


class carriageParm {
/*----------------------------------------------------------------------------
   The parameter to a client for an individual RPC.  It tells specifics
   of how to carry the call to the server and the response back.  For
   example, it may identify the server.  It may identify communication
   protocols to use.  It may indicate permission and accounting
   information.

   This is a base class; the carriage parameter is specific to the
   class of client.  For example, an HTTP-based client would have a
   URL and HTTP basic authentication info as parameter.
-----------------------------------------------------------------------------*/
protected:
    virtual void dummy() {}   // To make it polymorphic
    carriageParm() {};  // Don't let anyone instantiate this base class
};



class carriageParm_http0 : public carriageParm {

public:
    carriageParm_http0(std::string const serverUrl);

    virtual ~carriageParm_http0();

    virtual void
    setBasicAuth(std::string const userid,
                 std::string const password);

    xmlrpc_server_info * c_serverInfoP;

protected:
    carriageParm_http0();

    void
    instantiate(std::string const serverUrl);
};


class carriageParm_curl0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_curl0(std::string const serverUrl);

};


class carriageParm_libwww0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_libwww0(std::string const serverUrl);

};


class carriageParm_wininet0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_wininet0(std::string const serverUrl);

};


class clientXmlTransport {
/*----------------------------------------------------------------------------
   An object which transports XML to and from an XML-RPC server for an
   XML-RPC client.

   This is a base class.  Derived classes define methods to perform the
   transportation in particular ways.
-----------------------------------------------------------------------------*/
public:
    virtual ~clientXmlTransport();

    virtual void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP) = 0;
};



class clientXmlTransport_http : public xmlrpc_c::clientXmlTransport {
/*----------------------------------------------------------------------------
   A base class for client XML transports that use the simple, classic
   HTTP transports, all of which use the same carriage parameter.
-----------------------------------------------------------------------------*/
public:
    virtual ~clientXmlTransport_http();
    
    void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP);
    
protected:
    clientXmlTransport_http() {} // ensure no one can create
    struct xmlrpc_client_transport *           c_transportP;
    const struct xmlrpc_client_transport_ops * c_transportOpsP;
};



class clientXmlTransport_curl : public xmlrpc_c::clientXmlTransport_http {

public:
    clientXmlTransport_curl(std::string const networkInterface = "",
                            bool        const noSslVerifyPeer = false,
                            bool        const noSslVerifyHost = false);

    ~clientXmlTransport_curl();
};



class clientXmlTransport_libwww : public xmlrpc_c::clientXmlTransport_http {
    
public:
    clientXmlTransport_libwww(std::string const appname = "",
                              std::string const appversion = "");

    ~clientXmlTransport_libwww();
};



class clientXmlTransport_wininet : public xmlrpc_c::clientXmlTransport_http {

public:
    clientXmlTransport_wininet(bool const allowInvalidSslCerts = false);

    ~clientXmlTransport_wininet();
};



class client {
/*----------------------------------------------------------------------------
   A generic client -- a means of performing an RPC.  This is so generic
   that it can be used for clients that are not XML-RPC.

   This is a base class.  Derived classes define things such as that
   XML and HTTP get used to perform the RPC.
-----------------------------------------------------------------------------*/
public:
    virtual void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const  methodName,
         xmlrpc_c::paramList      const& paramList,
         xmlrpc_c::value *        const  resultP) = 0;
};



class clientXml : public xmlrpc_c::client {
/*----------------------------------------------------------------------------
   A client that uses XML-RPC XML in the RPC.  This class does not define
   how the XML gets transported, though (i.e. does not require HTTP).
-----------------------------------------------------------------------------*/
public:
    clientXml(xmlrpc_c::clientXmlTransport * const transportP);

    virtual void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const  methodName,
         xmlrpc_c::paramList      const& paramList,
         xmlrpc_c::value *        const  resultP);

private:
    xmlrpc_c::clientXmlTransport * transportP;
};


class connection {
/*----------------------------------------------------------------------------
   A nexus of a particular client and a particular server, along with
   carriage parameters for performing RPCs between the two.

   This is a minor convenience for client programs that always talk to
   the same server the same way.

   Use this as a parameter to rpc.call().
-----------------------------------------------------------------------------*/
public:
    connection(xmlrpc_c::client *       const clientP,
               xmlrpc_c::carriageParm * const carriageParmP);

    ~connection();

    xmlrpc_c::client *       clientP;
    xmlrpc_c::carriageParm * carriageParmP;
};

class rpc {
/*----------------------------------------------------------------------------
   An RPC.  An RPC consists of method name, parameters, and result.  It
   does not specify in any way how the method name and parameters get
   turned into a result.  It does not presume XML or HTTP.
-----------------------------------------------------------------------------*/
public:
    rpc(std::string          const  methodName,
        xmlrpc_c::paramList  const& paramList);

    virtual ~rpc();

    void
    call(xmlrpc_c::client       * const clientP,
         xmlrpc_c::carriageParm * const carriageParmP);

    void
    call(xmlrpc_c::connection const& connection);
    
    void
    start(xmlrpc_c::client       * const clientP,
          xmlrpc_c::carriageParm * const carriageParmP);
    
    void
    start(xmlrpc_c::connection const& connection);
    
    virtual void
    finish();

    xmlrpc_c::value
    getResult() const;

    bool
    isFinished() const;

private:
    bool finished;
    std::string methodName;
    xmlrpc_c::paramList paramList;
    xmlrpc_c::value result;
};

class clientSimple {

public:
    clientSimple();

    ~clientSimple();

    void
    call(std::string       const serverUrl,
         std::string       const methodName,
         xmlrpc_c::value * const resultP);

    void
    call(std::string       const serverUrl,
         std::string       const methodName,
         std::string       const format,
         xmlrpc_c::value * const resultP,
         ...);

    void
    call(std::string         const  serverUrl,
         std::string         const  methodName,
         xmlrpc_c::paramList const& paramList,
         xmlrpc_c::value *   const  resultP);

private:
    client * clientP;
    xmlrpc_c::clientXmlTransport * transportP;
};

} // namespace
#endif

#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <string>

#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/girmem.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/timeout.hpp>
#include <xmlrpc-c/client.h>

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
    virtual ~carriageParm();
    carriageParm();
};

class clientTransactionPtr;

class clientTransaction : public girmem::autoObject {

    friend class clientTransactionPtr;

public:
    virtual void
    finish(xmlrpc_c::rpcOutcome const& outcome) = 0;
    
    virtual void
    finishErr(girerr::error const& error) = 0;

protected:
    clientTransaction();
};

class clientTransactionPtr : public girmem::autoObjectPtr {
    
public:
    clientTransactionPtr();
    virtual ~clientTransactionPtr();

    virtual xmlrpc_c::clientTransaction *
    operator->() const;
};

class client {
/*----------------------------------------------------------------------------
   A generic client -- a means of performing an RPC.  This is so generic
   that it can be used for clients that are not XML-RPC.

   This is a base class.  Derived classes define things such as that
   XML and HTTP get used to perform the RPC.
-----------------------------------------------------------------------------*/
public:
    virtual ~client();

    virtual void
    call(carriageParm *         const  carriageParmP,
         std::string            const  methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP) = 0;

    virtual void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const  methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);
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

class carriageParm_http0 : public carriageParm {

public:
    carriageParm_http0(std::string const serverUrl);

    ~carriageParm_http0();

    void
    setBasicAuth(std::string const userid,
                 std::string const password);

    xmlrpc_server_info * c_serverInfoP;

protected:
    // Only a derived class is allowed to create an object with no
    // server URL, and the derived class expected to follow it up
    // with an instantiate() to establish the server URL.

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

class xmlTransactionPtr;

class xmlTransaction : public girmem::autoObject {

    friend class xmlTransactionPtr;

public:
    virtual void
    finish(std::string const& responseXml) const;

    virtual void
    finishErr(girerr::error const& error) const;

protected:
    xmlTransaction();
};

class xmlTransactionPtr : public girmem::autoObjectPtr {
public:
    xmlTransactionPtr();

    xmlrpc_c::xmlTransaction *
    operator->() const;
};

class clientXmlTransport : public girmem::autoObject {
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

    virtual void
    start(xmlrpc_c::carriageParm *    const  carriageParmP,
          std::string                 const& callXml,
          xmlrpc_c::xmlTransactionPtr const& xmlTranP);

    virtual void
    finishAsync(xmlrpc_c::timeout const timeout);

    static void
    asyncComplete(
        struct xmlrpc_call_info * const callInfoP,
        xmlrpc_mem_block *        const responseXmlMP,
        xmlrpc_env                const transportEnv);
};

class clientXmlTransportPtr : public girmem::autoObjectPtr {
    
public:
    clientXmlTransportPtr();

    clientXmlTransportPtr(xmlrpc_c::clientXmlTransport * const transportP);

    xmlrpc_c::clientXmlTransport *
    operator->() const;

    xmlrpc_c::clientXmlTransport *
    getp() const;
};

class clientXmlTransport_http : public xmlrpc_c::clientXmlTransport {
/*----------------------------------------------------------------------------
   A base class for client XML transports that use the simple, classic
   C HTTP transports.
-----------------------------------------------------------------------------*/
public:
    virtual ~clientXmlTransport_http();
    
    void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP);
    
    void
    start(xmlrpc_c::carriageParm *    const  carriageParmP,
          std::string                 const& callXml,
          xmlrpc_c::xmlTransactionPtr const& xmlTranP);
        
    virtual void
    finishAsync(xmlrpc_c::timeout const timeout);

protected:
    clientXmlTransport_http() {} // ensure no one can create
    struct xmlrpc_client_transport *           c_transportP;
    const struct xmlrpc_client_transport_ops * c_transportOpsP;
};


class clientXmlTransport_curl : public xmlrpc_c::clientXmlTransport_http {

public:
    class constrOpt {
    public:
        constrOpt();

        constrOpt & network_interface (string const& arg);
        constrOpt & no_ssl_verifypeer (bool   const& arg);
        constrOpt & no_ssl_verifyhost (bool   const& arg);
        constrOpt & user_agent        (string const& arg);
        constrOpt & ssl_cert          (string const& arg);
        constrOpt & sslcerttype       (string const& arg);
        constrOpt & sslcertpasswd     (string const& arg);
        constrOpt & sslkey            (string const& arg);
        constrOpt & sslkeytype        (string const& arg);
        constrOpt & sslkeypasswd      (string const& arg);
        constrOpt & sslengine         (string const& arg);
        constrOpt & sslengine_default (bool   const& arg);
        constrOpt & sslversion        (xmlrpc_sslversion const& arg);
        constrOpt & cainfo            (string const& arg);
        constrOpt & capath            (string const& arg);
        constrOpt & randomfile        (string const& arg);
        constrOpt & egdsocket         (string const& arg);
        constrOpt & ssl_cipher_list   (string const& arg);

        struct {
            string network_interface;
            bool   no_ssl_verifypeer;
            bool   no_ssl_verifyhost;
            string user_agent;
            string ssl_cert;
            string sslcerttype;
            string sslcertpasswd;
            string sslkey;
            string sslkeytype;
            string sslkeypasswd;
            string sslengine;
            bool   sslengine_default;
            xmlrpc_sslversion sslversion;
            string cainfo;
            string capath;
            string randomfile;
            string egdsocket;
            string ssl_cipher_list;
        } value;
        struct {
            bool network_interface;
            bool no_ssl_verifypeer;
            bool no_ssl_verifyhost;
            bool user_agent;
            bool ssl_cert;
            bool sslcerttype;
            bool sslcertpasswd;
            bool sslkey;
            bool sslkeytype;
            bool sslkeypasswd;
            bool sslengine;
            bool sslengine_default;
            bool sslversion;
            bool cainfo;
            bool capath;
            bool randomfile;
            bool egdsocket;
            bool ssl_cipher_list;
        } present;
    };

    clientXmlTransport_curl(constrOpt const& opt);

    clientXmlTransport_curl(std::string const networkInterface = "",
                            bool        const noSslVerifyPeer = false,
                            bool        const noSslVerifyHost = false,
                            std::string const userAgent = "");

    ~clientXmlTransport_curl();

private:
    void
    initialize(constrOpt const& opt);
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

class client_xml : public xmlrpc_c::client {
/*----------------------------------------------------------------------------
   A client that uses XML-RPC XML in the RPC.  This class does not define
   how the XML gets transported, though (i.e. does not require HTTP).
-----------------------------------------------------------------------------*/
public:
    client_xml(xmlrpc_c::clientXmlTransport * const transportP);

    client_xml(xmlrpc_c::clientXmlTransportPtr const transportP);

    void
    call(carriageParm *         const  carriageParmP,
         std::string            const  methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP);

    void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const  methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finishAsync(xmlrpc_c::timeout const timeout);

private:
    /* We have both kinds of pointers to give the user flexibility -- we
       have constructors that take both.  But the simple pointer
       'transportP' is valid in both cases.
    */
    xmlrpc_c::clientXmlTransport * transportP;
    xmlrpc_c::clientXmlTransportPtr transportPtr;
};

class xmlTransaction_client : public xmlrpc_c::xmlTransaction {

public:
    xmlTransaction_client(xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finish(std::string const& responseXml) const;

    void
    finishErr(girerr::error const& error) const;
private:
    xmlrpc_c::clientTransactionPtr const tranP;
};

class xmlTransaction_clientPtr : public xmlTransactionPtr {
public:
    xmlTransaction_clientPtr();
    
    xmlTransaction_clientPtr(xmlrpc_c::clientTransactionPtr const& tranP);

    xmlrpc_c::xmlTransaction_client *
    operator->() const;
};

class rpcPtr;

class rpc : public clientTransaction {
/*----------------------------------------------------------------------------
   An RPC.  An RPC consists of method name, parameters, and result.  It
   does not specify in any way how the method name and parameters get
   turned into a result.  It does not presume XML or HTTP.

   You don't create an object of this class directly.  All references to
   an rpc object should be by an rpcPtr object.  Create a new RPC by
   creating a new rpcPtr.  Accordingly, our constructors and destructors
   are protected, but available to our friend class rpcPtr.

   In order to do asynchronous RPCs, you normally have to create a derived
   class that defines a useful notifyComplete().  If you do that, you'll
   want to make sure the derived class objects get accessed only via rpcPtrs
   as well.
-----------------------------------------------------------------------------*/
    friend class xmlrpc_c::rpcPtr;

public:
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
    
    void
    finish(xmlrpc_c::rpcOutcome const& outcome);

    void
    finishErr(girerr::error const& error);

    virtual void
    notifyComplete();

    bool
    isFinished() const;

    bool
    isSuccessful() const;

    xmlrpc_c::value
    getResult() const;

    xmlrpc_c::fault
    getFault() const;

protected:
    rpc(std::string         const  methodName,
        xmlrpc_c::paramList const& paramList);

    virtual ~rpc();

private:
    enum state {
        STATE_UNFINISHED,  // RPC is running or not started yet
        STATE_ERROR,       // We couldn't execute the RPC
        STATE_FAILED,      // RPC executed successfully, but failed per XML-RPC
        STATE_SUCCEEDED    // RPC is done, no exception
    };
    enum state state;
    girerr::error * errorP;     // Defined only in STATE_ERROR
    xmlrpc_c::rpcOutcome outcome;
        // Defined only in STATE_FAILED and STATE_SUCCEEDED
    std::string methodName;
    xmlrpc_c::paramList paramList;
};

class rpcPtr : public clientTransactionPtr {
public:
    rpcPtr();

    rpcPtr(xmlrpc_c::rpc * const rpcP);

    rpcPtr(std::string         const  methodName,
           xmlrpc_c::paramList const& paramList);

    xmlrpc_c::rpc *
    operator->() const;
};

} // namespace
#endif

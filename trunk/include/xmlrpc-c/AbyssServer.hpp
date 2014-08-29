#ifndef ABYSS_SERVER_HPP_INCLUDED
#define ABYSS_SERVER_HPP_INCLUDED

#include <stdexcept>

#include <vector>
#include <string>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/AbyssChanSwitch.hpp>

class AbyssServer {

public:

    class Exception : public std::runtime_error {
    public:
        Exception(unsigned short const  httpStatusCode,
                  string         const& explanation);

        unsigned short
        httpStatusCode() const;

    private:
        unsigned short _httpStatusCode;
    };

    class Session {
    public:
        Session(TSession * const cSessionP);

        enum Method {
            METHOD_UNKNOWN,
            METHOD_GET,
            METHOD_PUT,
            METHOD_HEAD,
            METHOD_POST,
            METHOD_DELETE,
            METHOD_TRACE,
            METHOD_OPTIONS
        };

        Method
        method() const;

        std::string const
        requestLine() const;

        std::string const
        uriPathName() const;

        std::vector<std::string> const
        uriPathNameSegment() const;

        bool
        uriHasQuery() const;

        std::string const
        uriQuery() const;

        std::map<std::string, std::string> const
        formInput() const;

        bool
        hasHost() const;

        std::string const
        host() const;

        bool
        hasFrom() const;

        std::string const
        from() const;

        bool
        hasUseragent() const;

        std::string const
        useragent() const;

        bool
        hasReferer() const;

        std::string const
        referer() const;

        bool
        userIsAuthenticated() const;

        std::string const
        user() const;

        unsigned short
        port() const;

        bool
        keepalive() const;

        void
        setRespStatus(unsigned short const statusCode);

        void
        setRespContentType(std::string const& contentType);

        void
        setRespContentLength(uint64_t const contentLength);

        void
        startWriteResponse();

        void
        endWriteResponse();

        void
        writeResponseBody(std::string const& bodyPart);

        void
        writeResponse(std::string const& body);

        void
        sendErrorResponse(std::string const& explanation);

        void
        sendErrorResponse(Exception const& e);

    private:
        
        TSession * const cSessionP;

        bool responseStarted;
            // We've started (and possibly finished) a response in this
            // session.
    };

    class ReqHandler {

    public:
        virtual
        ~ReqHandler();

        virtual void
        handleRequest(Session * const sessionP,
                      bool *    const handledP) = 0;

        virtual size_t
        handleReqStackSize() const;

        virtual void
        terminate();
    };

    AbyssServer(AbyssChanSwitch * const chanSwitchP);

    ~AbyssServer();

    void
    addRequestHandler(ReqHandler * const handlerP);

    void
    init();
    
    void
    run();

    void
    runOnce();

private:
    TServer cServer;
};

#endif

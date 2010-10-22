#ifndef XMLRPC_TIMEOUT_H_INCLUDED
#define XMLRPC_TIMEOUT_H_INCLUDED

namespace xmlrpc_c {

#ifndef XMLRPC_DLLEXPORT
#define XMLRPC_DLLEXPORT /* as nothing */
#endif

struct XMLRPC_DLLEXPORT timeout {

    timeout() : finite(false) {}

    timeout(unsigned int const duration) :
        finite(true), duration(duration) {}
        // 'duration' is the timeout time in milliseconds

    bool finite;
    unsigned int duration;  // in milliseconds
};


} // namespace

#endif

#ifndef XMLRPC_C_CONFIG_H_INCLUDED
#define XMLRPC_C_CONFIG_H_INCLUDED

/* This file, part of XML-RPC For C/C++, is meant to 
   define characteristics of this particular installation 
   that the other <xmlrpc-c/...> header files need in 
   order to compile correctly when #included in Xmlrpc-c
   user code.

   Those header files #include this one.

   This file was created by a make rule.
*/
#define XMLRPC_HAVE_WCHAR 1
#ifdef WIN32
  /* SOCKET is a type defined by <winsock.h>.  Anyone who
     uses XMLRPC_SOCKET on a WIN32 system must #include
     <winsock.h>
  */
  #define XMLRPC_SOCKET SOCKET
  #define XMLRPC_HAVE_TIMEVAL 0
  #define XMLRPC_HAVE_TIMESPEC 0
#else
  #define XMLRPC_SOCKET int
  #define XMLRPC_HAVE_TIMEVAL 1
  #define XMLRPC_HAVE_TIMESPEC 1
#endif

#if defined(_MSC_VER)
#if _MSC_VER < 1300
  /* This is MSVC 6. */
  #define XMLRPC_LONG_LONG __int64
#else
  #define XMLRPC_LONG_LONG long long
#endif
#else
  #define XMLRPC_LONG_LONG long long
#endif
#endif


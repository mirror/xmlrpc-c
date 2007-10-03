/* This file, part of XML-RPC For C/C++, is meant to 
   define characteristics of this particular installation 
   that the other <xmlrpc-c/...> header files need in 
   order to compile correctly when #included in a Xmlrpc-c
   user code.

   Those header files #include this one.
*/

#ifndef XMLRPC_HAVE_WCHAR
#define XMLRPC_HAVE_WCHAR 1
#endif   // #ifndef XMLRPC_HAVE_WCHAR

#define  ATTR_UNUSED

#ifdef   _MSC_VER
#define  snprintf _snprintf
#endif /* _MSC_VER */

// eof - config.h

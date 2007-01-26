/* This file, part of XML-RPC For C/C++, is meant to 
   define characteristics of this particular installation 
   that the other <xmlrpc-c/...> header files need in 
   order to compile correctly when #included in a Xmlrpc-c
   user code.

   Those header files #include this one.
*/
#define XMLRPC_HAVE_WCHAR 1
#ifndef HAVE_UNICODE_WCHAR
#define HAVE_UNICODE_WCHAR 1
#endif	

#define ATTR_UNUSED

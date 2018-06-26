#ifndef XMLRPC_PARSE_H_INCLUDED
#define XMLRPC_PARSE_H_INCLUDED
/*=============================================================================
                              xmlrpc_parse.h
===============================================================================
  This declares the interface to the _internal_ XML-RPC XML parsing functions
  in xmlrpc_parse.c.

  There are also _external_ functions in xmlrpc_parse.c, and those are
  declared in an external interface header file.

  xmlrpc_parse.c is split that way because some functions use memory pools,
  which is an internal concept we don't feel like making external right now.
=============================================================================*/

#include "xmlrpc-c/util_int.h"

void 
xmlrpc_parse_call2(xmlrpc_env *      const envP,
                   const char *      const xmlData,
                   size_t            const xmlDataLen,
                   xmlrpc_mem_pool * const memPoolP,
                   const char **     const methodNameP,
                   xmlrpc_value **   const paramArrayPP);

void
xmlrpc_parse_response3(xmlrpc_env *      const envP,
                       const char *      const xmlData,
                       size_t            const xmlDataLen,
                       xmlrpc_mem_pool * const memPoolP,
                       xmlrpc_value **   const resultPP,
                       int *             const faultCodeP,
                       const char **     const faultStringP);

void
xmlrpc_parse_value_xml2(xmlrpc_env *      const envP,
                        const char *      const xmlData,
                        size_t            const xmlDataLen,
                        xmlrpc_mem_pool * const memPoolP,
                        xmlrpc_value **   const valuePP);

#endif

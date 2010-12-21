#ifndef STRING_NUMBER_H_INCLUDED
#define STRING_NUMBER_H_INCLUDED

#include <xmlrpc-c/config.h>
#include <xmlrpc-c/util.h>

void
xmlrpc_parse_int64(xmlrpc_env *   const envP,
                   const char *   const str,
                   xmlrpc_int64 * const i64P);

#endif

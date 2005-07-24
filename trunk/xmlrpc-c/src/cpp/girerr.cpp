#define _GNU_SOURCE
#include <string>

#include "xmlrpc-c/girerr.hpp"
#include "xmlrpc-c/base_int.h"

namespace girerr {

void
throwf(const char * const format, ...) {

    va_list varargs;
    va_start(varargs, format);

    const char * value;
    xmlrpc_vasprintf(&value, format, varargs);
    
    string const valueString(value);

    xmlrpc_strfree(value);

    throw(girerr::error(valueString));

    va_end(varargs);
}

} // namespace

#define _GNU_SOURCE

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"



static __inline__ void
simpleVasprintf(char **      const retvalP,
                const char * const fmt,
                va_list            varargs) {
/*----------------------------------------------------------------------------
   This is a poor man's implementation of vasprintf(), of GNU fame.
-----------------------------------------------------------------------------*/
    size_t const initialSize = 4096;
    char * result;

    result = malloc(initialSize);
    if (result != NULL) {
        size_t bytesNeeded;
        bytesNeeded = vsnprintf(result, initialSize, fmt, varargs);
        if (bytesNeeded > initialSize) {
            free(result);
            result = malloc(bytesNeeded);
            if (result != NULL)
                vsnprintf(result, bytesNeeded, fmt, varargs);
        } else if (bytesNeeded == initialSize) {
            if (result[initialSize-1] != '\0') {
                /* This is one of those old systems where vsnprintf()
                   returns the number of bytes it used, instead of the
                   number that it needed, and it in fact needed more than
                   we gave it.  Rather than mess with this highly unlikely
                   case (old system and string > 4095 characters), we just
                   treat this like an out of memory failure.
                */
                free(result);
                result = NULL;
            }
        }
    }
    *retvalP = result;
}



void
xmlrpc_vasprintf(const char ** const retvalP,
                 const char *  const fmt,
                 va_list             varargs) {

    char * retval;

#if HAVE_ASPRINTF
    vasprintf(&retval, fmt, varargs);
#else
    simpleVasprintf(&retval, fmt, varargs);
#endif

    *retvalP = retval;
}


void GNU_PRINTF_ATTR(2,3)
xmlrpc_asprintf(const char ** const retvalP, const char * const fmt, ...) {

    va_list varargs;  /* mysterious structure used by variable arg facility */

    va_start(varargs, fmt); /* start up the mysterious variable arg facility */

    xmlrpc_vasprintf(retvalP, fmt, varargs);

    va_end(varargs);
}



void
xmlrpc_strfree(const char * const string) {
    free((void *)string);
}
const char * 
xmlrpc_makePrintable(const char * const input) {
/*----------------------------------------------------------------------------
   Convert an arbitrary string of bytes (null-terminated, though) to
   printable ASCII.  E.g. convert newlines to "\n".

   Return the result in newly malloc'ed storage.  Return NULL if we can't
   get the storage.
-----------------------------------------------------------------------------*/
    char * output;
    const unsigned int inputLength = strlen(input);

    output = malloc(inputLength*4+1);

    if (output != NULL) {
        unsigned int inputCursor, outputCursor;

        for (inputCursor = 0, outputCursor = 0; 
             inputCursor < inputLength; 
             ++inputCursor) {

            if (isprint(input[inputCursor]))
                output[outputCursor++] = input[inputCursor]; 
            else if (input[inputCursor] == '\n') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'n';
            } else if (input[inputCursor] == '\t') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 't';
            } else if (input[inputCursor] == '\a') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'a';
            } else if (input[inputCursor] == '\r') {
                output[outputCursor++] = '\\';
                output[outputCursor++] = 'r';
            } else {
                snprintf(&output[outputCursor], 4, "\\x%02x", 
                         input[inputCursor]);
            }
        }
        output[outputCursor++] = '\0';
    }
    return output;
}



const char *
xmlrpc_makePrintableChar(char const input) {

    const char * retval;

    if (input == '\0')
        retval = strdup("\\0");
    else {
        char buffer[2];
        
        buffer[0] = input;
        buffer[1] = '\0';
        
        retval = xmlrpc_makePrintable(buffer);
    }
    return retval;
}

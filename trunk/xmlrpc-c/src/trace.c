#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "xmlrpc-c/base_int.h"



void
xmlrpc_traceXml(const char * const label, 
                const char * const xml,
                unsigned int const xmlLength) {

    if (getenv("XMLRPC_TRACE_XML")) {
        unsigned int nonPrintableCount;
        unsigned int i;

        fprintf(stderr, "%s:\n\n", label);

        nonPrintableCount = 0;  /* Initial value */

        for (i = 0; i < xmlLength; ++i) {
            char const thisChar = xml[i];
            if (!isprint(thisChar) && thisChar != '\n' && thisChar != '\r') {
                ++nonPrintableCount;
                fputc('~', stderr);
            } else
                fputc(thisChar, stderr);
        }
        fputc('\n', stderr);
        if (nonPrintableCount > 0)
            fprintf(stderr, "%s contains %u nonprintable characters.\n", 
                    label, nonPrintableCount);
    }
}


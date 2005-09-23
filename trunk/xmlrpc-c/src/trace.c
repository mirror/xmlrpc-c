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

        nonPrintableCount = 0;  /* Initial value */

        for (i = 0; i < xmlLength; ++i) {
            if (!isprint(xml[i]) && xml[i] != '\n' && xml[i] != '\r')
                ++nonPrintableCount;
        }
        if (nonPrintableCount > 0)
            fprintf(stderr, "%s contains %u nonprintable characters.\n", 
                    label, nonPrintableCount);

        fprintf(stderr, "%s:\n\n", label);
        fprintf(stderr, "%.*s\n", (int)xmlLength, xml);
    }
}


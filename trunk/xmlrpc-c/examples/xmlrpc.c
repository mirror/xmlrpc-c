/* Make an XML-RPC call.

   User specifies details of the call on the command line.

   We print the result on Standard Output.

   Example:

     $ xmlrpc http://localhost:8080/RPC2 sample.add i/3 i/5
     Result:
       Integer: 8

     $ xmlrpc localhost:8080 sample.add i/3 i/5
     Result:
       Integer: 8

   This is just the beginnings of this program.  It should be extended
   to deal with all types of parameters and results.

   An example of a good syntax for parameters would be:

     $ xmlrpc http://www.oreillynet.com/meerkat/xml-rpc/server.php \
         meerkat.getItems \
         struct/(search:linux,descriptions:i/76,time_period:12hour)
     Result:
       String: 

     $ xmlrpc http://www.oreillynet.com/meerkat/xml-rpc/server.php \
         meerkat.getItems \
         struct/{search:linux,descriptions:i/76,time_period:12hour}
     Result:
       Array:
         Struct:
           title: String: DatabaseJournal: OpenEdge-Based Finance ...
           link: String: http://linuxtoday.com/news_story.php3?ltsn=...
           description: String: "Finance application with embedded ...
         Struct:
           title: ...
           link: ...
           description: ...

*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xmlrpc.h>
#include <xmlrpc_client.h>

#include "config.h"  /* information about this build environment */

#define NAME "xmlrpc command line program"
#define VERSION "1.0"

struct cmdlineInfo {
    const char *  url;
    const char *  methodName;
    unsigned int  paramCount;
    const char ** params;
};



/* GNU_PRINTF_ATTR lets the GNU compiler check pm_message() and pm_error()
   calls to be sure the arguments match the format string, thus preventing
   runtime segmentation faults and incorrect messages.
*/
#ifdef __GNUC__
#define GNU_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
#define GNU_PRINTF_ATTR(a,b)
#endif

static void GNU_PRINTF_ATTR(2,3)
casprintf(const char ** const retvalP, const char * const fmt, ...) {

    char *retval;

    va_list varargs;  /* mysterious structure used by variable arg facility */

    va_start(varargs, fmt); /* start up the mysterious variable arg facility */

#if HAVE_ASPRINTF
    vasprintf(&retval, fmt, varargs);
#else
    retval = malloc(8192);
    vsnprintf(retval, 8192, fmt, varargs);
#endif
    *retvalP = retval;
}



static void
strfree(const char * const string) {
    free((void *)string);
}



static void 
die_if_fault_occurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "Error: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



static void GNU_PRINTF_ATTR(2,3)
error(xmlrpc_env * const envP, const char format[], ...) {
    va_list args;
    char * faultString;

    va_start(args, format);

    vasprintf(&faultString, format, args);
    va_end(args);

    xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR, faultString);

    free(faultString);
}
      


static void
parseCommandLine(xmlrpc_env *         const envP,
                 int                  const argc,
                 const char **        const argv,
                 struct cmdlineInfo * const cmdlineP) {
                 
    if (argc-1 < 2)
        error(envP, "Not enough arguments.  Need at least a URL and "
              "method name.");
    else {
        cmdlineP->url = argv[1];
        cmdlineP->methodName = argv[2];
        cmdlineP->paramCount = argc-1 - 2;
        cmdlineP->params = &argv[3];
    }
}



static void
computeUrl(const char *  const urlArg,
           const char ** const urlP) {

    if (strstr(urlArg, "://") != 0) {
        *urlP = strdup(urlArg);
    } else {
        casprintf(urlP, "http://%s/RPC2", urlArg);
    }        
}



static void
computeParameter(xmlrpc_env *    const envP,
                 const char *    const paramArg,
                 xmlrpc_value ** const paramPP) {

    if (strncmp(paramArg, "s/", 2) == 0) {
        /* It's "s/..." -- a string */
        *paramPP = xmlrpc_build_value(envP, "s", &paramArg[2]);
    } else if (strncmp(paramArg, "i/", 2) == 0) {
        /* It's "i/..." -- an integer */
        const char * integerString = &paramArg[2];

        if (strlen(integerString) < 1)
            error(envP, "Integer argument has nothing after the 'i/'");
        else {
            long value;
            char * tailptr;

            value = strtol(integerString, &tailptr, 10);

            if (*tailptr != '\0')
                error(envP, "Integer argument has non-digit crap in it: '%s'",
                      tailptr);
            else
                *paramPP = xmlrpc_build_value(envP, "i", value);
        }
    } else {
        /* It's not in normal type/value format, so we take it to be
           the shortcut string notation 
        */
        *paramPP = xmlrpc_build_value(envP, "s", paramArg);
    }

}



static void
computeParamArray(xmlrpc_env *    const envP,
                  unsigned int    const paramCount,
                  const char **   const params,
                  xmlrpc_value ** const paramArrayPP) {
    
    unsigned int i;

    xmlrpc_value * paramArrayP;

    paramArrayP = xmlrpc_build_value(envP, "()");

    for (i = 0; i < paramCount && !envP->fault_occurred; ++i) {
        xmlrpc_value * paramP;

        computeParameter(envP, params[i], &paramP);

        if (!envP->fault_occurred) {
            xmlrpc_array_append_item(envP, paramArrayP, paramP);

            xmlrpc_DECREF(paramP);
        }
    }
    *paramArrayPP = paramArrayP;
}



static void
dumpResult(xmlrpc_value * const resultP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    printf("Result:\n\n");

    switch (xmlrpc_value_type(resultP)) {
    case XMLRPC_TYPE_INT: {
        int value;
        
        xmlrpc_parse_value(&env, resultP, "i", &value);
        
        if (env.fault_occurred)
            printf("Unable to parse integer in result.  %s\n",
                   env.fault_string);
        else
            printf("Integer: %d\n", value);
    }
    break;
    case XMLRPC_TYPE_STRING: {
        const char * value;

        xmlrpc_parse_value(&env, resultP, "s", &value);
        
        if (env.fault_occurred)
            printf("Unable to parse string in result.  %s\n",
                   env.fault_string);
        else
            printf("String: '%s'\n", value);
    }
    break;
    default:
        printf("Don't recognize the type of the result: %u.\n", 
               xmlrpc_value_type(resultP));
        printf("Can't print it.\n");
    }

    xmlrpc_env_clean(&env);
}



static void
doCall(xmlrpc_env *    const envP,
       const char *    const url,
       const char *    const methodName,
       xmlrpc_value *  const paramArrayP,
       xmlrpc_value ** const resultPP) {
    
    XMLRPC_ASSERT(xmlrpc_value_type(paramArrayP) == XMLRPC_TYPE_ARRAY);

    xmlrpc_client_init2(envP, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    if (!envP->fault_occurred) {
        *resultPP = xmlrpc_client_call_params(
            envP, url, methodName, paramArrayP);
    
        xmlrpc_client_cleanup();
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    struct cmdlineInfo cmdline;
    xmlrpc_env env;
    xmlrpc_value * paramArrayP;
    xmlrpc_value * resultP;
    const char * url;

    xmlrpc_env_init(&env);

    parseCommandLine(&env, argc, argv, &cmdline);
    die_if_fault_occurred(&env);

    computeUrl(cmdline.url, &url);

    computeParamArray(&env, cmdline.paramCount, cmdline.params, &paramArrayP);
    die_if_fault_occurred(&env);

    doCall(&env, url, cmdline.methodName, paramArrayP, &resultP);
    die_if_fault_occurred(&env);

    dumpResult(resultP);
    
    strfree(url);

    xmlrpc_DECREF(resultP);

    xmlrpc_env_clean(&env);
    
    return 0;
}

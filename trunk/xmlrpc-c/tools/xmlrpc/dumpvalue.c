/* dumpvalue() service, which prints to Standard Output the value of
   an xmlrpc_value.

   We've put this in a separate module in hopes that it eventually can be
   used for debugging purposes in other places.
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xmlrpc_config.h"  /* information about this build environment */
#include "casprintf.h"
#include "mallocvar.h"

#include "xmlrpc-c/base.h"

#include "dumpvalue.h"



static void
dumpInt(const char *   const prefix,
        xmlrpc_value * const valueP) {

    xmlrpc_env env;
    xmlrpc_int value;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_int(&env, valueP, &value);
    
    if (env.fault_occurred)
        printf("Internal error: unable to extract value of "
               "integer xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%sInteger: %d\n", prefix, value);

    xmlrpc_env_clean(&env);
}



static void
dumpBool(const char *   const prefix,
         xmlrpc_value * const valueP) {

    xmlrpc_env env;
    xmlrpc_bool value;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_bool(&env, valueP, &value);
    
    if (env.fault_occurred)
        printf("Internal error: Unable to extract value of "
               "boolean xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%sBoolean: %s\n", prefix, value ? "TRUE" : "FALSE");

    xmlrpc_env_clean(&env);
}



static void
dumpDouble(const char *   const prefix,
           xmlrpc_value * const valueP) {

    xmlrpc_env env;
    xmlrpc_double value;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_double(&env, valueP, &value);
    
    if (env.fault_occurred)
        printf("Internal error: Unable to extract value from "
               "floating point number xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%sFloating Point: %f\n", prefix, value);

    xmlrpc_env_clean(&env);
}



static void
dumpDatetime(const char *   const prefix,
             xmlrpc_value * const valueP) {

    printf("%sDon't know how to print datetime value %lx.\n", 
           prefix, (unsigned long) valueP);
}



static void
dumpString(const char *   const prefix,
           xmlrpc_value * const valueP) {

    xmlrpc_env env;
    size_t length;
    const char * value;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_string_lp(&env, valueP, &length, &value);
    
    if (env.fault_occurred)
        printf("Internal error: Unable to extract value from "
               "string xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else {
        if (strlen(value) != length)
            printf("String value contains a NUL character, which means the "
                   "XML-RPC response was not valid XML.  We report only up to "
                   "the NUL.\n");

        printf("%sString: '%s'\n", prefix, value);

        strfree(value);
    }
    xmlrpc_env_clean(&env);
}



static void
dumpBase64(const char *   const prefix,
           xmlrpc_value * const valueP) {

    xmlrpc_env env;
    const unsigned char * value;
    size_t length;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_base64(&env, valueP, &length, &value);
    
    if (env.fault_occurred)
        printf("Unable to parse base64 bit string xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else {
        unsigned int i;

        printf("%sBit string: ", prefix);
        for (i = 0; i < length; ++i)
            printf("%02x", value[i]);

        free((void*)value);
    }
    xmlrpc_env_clean(&env);
}



static void
dumpArray(const char *   const prefix,
          xmlrpc_value * const arrayP) {

    xmlrpc_env env;
    unsigned int arraySize;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    arraySize = xmlrpc_array_size(&env, arrayP);
    if (env.fault_occurred)
        printf("Unable to get array size.  %s\n", env.fault_string);
    else {
        int const spaceCount = strlen(prefix);

        unsigned int i;
        const char * blankPrefix;

        printf("%sArray of %u items:\n", prefix, arraySize);

        casprintf(&blankPrefix, "%*s", spaceCount, "");

        for (i = 0; i < arraySize; ++i) {
            xmlrpc_value * valueP;

            xmlrpc_array_read_item(&env, arrayP, i, &valueP);

            if (env.fault_occurred)
                printf("Unable to get array item %u\n", i);
            else {
                const char * prefix2;

                casprintf(&prefix2, "%s  Index %2u ", blankPrefix, i);
                dumpValue(prefix2, valueP);
                strfree(prefix2);

                xmlrpc_DECREF(valueP);
            }
        }
        strfree(blankPrefix);
    }
    xmlrpc_env_clean(&env);
}



static void
dumpStructMember(const char *   const prefix,
                 xmlrpc_value * const structP,
                 unsigned int   const index) {

    xmlrpc_env env;

    xmlrpc_value * keyP;
    xmlrpc_value * valueP;
    
    xmlrpc_env_init(&env);

    xmlrpc_struct_read_member(&env, structP, index, &keyP, &valueP);

    if (env.fault_occurred)
        printf("Unable to get struct member %u\n", index);
    else {
        int const blankCount = strlen(prefix);
        const char * prefix2;
        const char * blankPrefix;

        casprintf(&prefix2, "%s  Key:   ", prefix);
        dumpValue(prefix2, keyP);
        strfree(prefix2);

        casprintf(&blankPrefix, "%*s", blankCount, "");
        
        casprintf(&prefix2, "%s  Value: ", blankPrefix);
        dumpValue(prefix2, valueP);
        strfree(prefix2);

        strfree(blankPrefix);

        xmlrpc_DECREF(keyP);
        xmlrpc_DECREF(valueP);
    }
    xmlrpc_env_clean(&env);
}



static void
dumpStruct(const char *   const prefix,
           xmlrpc_value * const structP) {

    xmlrpc_env env;
    unsigned int structSize;

    xmlrpc_env_init(&env);

    structSize = xmlrpc_struct_size(&env, structP);
    if (env.fault_occurred)
        printf("Unable to get struct size.  %s\n", env.fault_string);
    else {
        unsigned int i;

        printf("%sStruct of %u members:\n", prefix, structSize);

        for (i = 0; i < structSize; ++i) {
            const char * prefix1;

            if (i == 0)
                prefix1 = strdup(prefix);
            else {
                int const blankCount = strlen(prefix);
                casprintf(&prefix1, "%*s", blankCount, "");
            }            
            dumpStructMember(prefix1, structP, i);

            strfree(prefix1);
        }
    }
    xmlrpc_env_clean(&env);
}



static void
dumpCPtr(const char *   const prefix,
         xmlrpc_value * const valueP) {

    xmlrpc_env env;
    void * value;

    xmlrpc_env_init(&env);

    xmlrpc_read_cptr(&env, valueP, &value);
        
    if (env.fault_occurred)
        printf("Unable to parse C pointer xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%sC pointer: '%lux'\n", prefix, (unsigned long)value);

    xmlrpc_env_clean(&env);
}



static void
dumpNil(const char *   const prefix,
        xmlrpc_value * const valueP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_read_nil(&env, valueP);
        
    if (env.fault_occurred)
        printf("Internal error: nil value xmlrpc_value %lx "
               "is not valid.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%sNil\n", prefix);

    xmlrpc_env_clean(&env);
}



static void
dumpI8(const char *   const prefix,
       xmlrpc_value * const valueP) {

    xmlrpc_env env;
    xmlrpc_int64 value;
    
    xmlrpc_env_init(&env);

    xmlrpc_read_i8(&env, valueP, &value);
    
    if (env.fault_occurred)
        printf("Internal error: unable to extract value of "
               "64-bit integer xmlrpc_value %lx.  %s\n",
               (unsigned long)valueP, env.fault_string);
    else
        printf("%s64-bit integer: %lld\n", prefix, value);

    xmlrpc_env_clean(&env);
}



static void
dumpUnknown(const char *   const prefix,
            xmlrpc_value * const valueP) {

    printf("%sDon't recognize value type %u of xmlrpc_value %lx.\n", 
           prefix, xmlrpc_value_type(valueP), (unsigned long)valueP);
    printf("%sCan't print it.\n", prefix);
}



void
dumpValue(const char *   const prefix,
          xmlrpc_value * const valueP) {

    switch (xmlrpc_value_type(valueP)) {
    case XMLRPC_TYPE_INT:
        dumpInt(prefix, valueP);
        break;
    case XMLRPC_TYPE_BOOL: 
        dumpBool(prefix, valueP);
        break;
    case XMLRPC_TYPE_DOUBLE: 
        dumpDouble(prefix, valueP);
        break;
    case XMLRPC_TYPE_DATETIME:
        dumpDatetime(prefix, valueP);
        break;
    case XMLRPC_TYPE_STRING: 
        dumpString(prefix, valueP);
        break;
    case XMLRPC_TYPE_BASE64:
        dumpBase64(prefix, valueP);
        break;
    case XMLRPC_TYPE_ARRAY: 
        dumpArray(prefix, valueP);
        break;
    case XMLRPC_TYPE_STRUCT:
        dumpStruct(prefix, valueP);
        break;
    case XMLRPC_TYPE_C_PTR:
        dumpCPtr(prefix, valueP);
        break;
    case XMLRPC_TYPE_NIL:
        dumpNil(prefix, valueP);
        break;
    case XMLRPC_TYPE_I8:
        dumpI8(prefix, valueP);
        break;
    default:
        dumpUnknown(prefix, valueP);
    }
}

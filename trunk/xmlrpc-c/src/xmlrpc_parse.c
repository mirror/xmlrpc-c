/* Copyright information is at end of file. */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/xmlparser.h"


/* Notes about XML-RPC XML documents:

   Contain CDATA: methodName, i4, int, boolean, string, double,
                  dateTime.iso8601, base64, name

   We attempt to validate the structure of the XML document carefully.
   We also try *very* hard to handle malicious data gracefully, and without
   leaking memory.

   The CHECK_NAME and CHECK_CHILD_COUNT macros examine an XML element, and
   invoke XMLRPC_FAIL if something looks wrong.
*/

#define CHECK_NAME(env,elem,name) \
    do \
        if (!xmlrpc_streq((name), xml_element_name(elem))) \
            XMLRPC_FAIL2(env, XMLRPC_PARSE_ERROR, \
             "Expected element of type <%s>, found <%s>", \
                         (name), xml_element_name(elem)); \
    while (0)

#define CHECK_CHILD_COUNT(env,elem,count) \
    do \
        if (xml_element_children_size(elem) != (count)) \
            XMLRPC_FAIL3(env, XMLRPC_PARSE_ERROR, \
             "Expected <%s> to have %d children, found %d", \
                         xml_element_name(elem), (count), \
                         xml_element_children_size(elem)); \
    while (0)

static xml_element *
get_child_by_name (xmlrpc_env *env, xml_element *parent, char *name)
{
    size_t child_count, i;
    xml_element **children;

    children = xml_element_children(parent);
    child_count = xml_element_children_size(parent);
    for (i = 0; i < child_count; i++) {
        if (xmlrpc_streq(xml_element_name(children[i]), name))
            return children[i];
    }
    
    xmlrpc_env_set_fault_formatted(env, XMLRPC_PARSE_ERROR,
                                   "Expected <%s> to have child <%s>",
                                   xml_element_name(parent), name);
    return NULL;
}



static void
parseFault(xmlrpc_env * const envP,
           const char * const format,
           ...) {

    va_list args;
    va_start(args, format);
    xmlrpc_set_fault_formatted_v(envP, XMLRPC_PARSE_ERROR, format, args);
    va_end(args);
}



static void
parseInt(xmlrpc_env *    const envP,
         const char *    const str,
         xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <int> XML-RPC XML element, e.g. "34".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (str[0] == '\0')
        parseFault(envP, "<int> XML element content is empty.");
    else if (isspace(str[0]))
        parseFault(envP, "<int> content '%s' starts with white space.", str);
    else {
        long i;
        char * tail;

        errno = 0;
        i = strtol(str, &tail, 10);

        /* Look for ERANGE. */
        if (errno == ERANGE)
            parseFault(envP, "<int> XML element value '%s' represents a "
                       "number beyond the range that "
                       "XML-RPC allows (%d - %d)", str,
                       XMLRPC_INT32_MIN, XMLRPC_INT32_MAX);
        else if (errno != 0)
            parseFault(envP, "unexpected error parsing <int> XML element "
                       "value '%s'.  strtol() failed with errno %d (%s)",
                       str, errno, strerror(errno));
        else {
            /* Look for out-of-range errors which didn't produce ERANGE. */
            if (i < XMLRPC_INT32_MIN)
                parseFault(envP, "<int> value %d is below the range allowed "
                           "by XML-RPC (minimum is %d)",
                           i, XMLRPC_INT32_MIN);
            else if (i > XMLRPC_INT32_MAX)
                parseFault(envP, "<int> value %d is above the range allowed "
                           "by XML-RPC (maximum is %d)",
                           i, XMLRPC_INT32_MAX);
            else {
                if (tail[0] != '\0')
                    parseFault(envP, "<int> value '%s' contains non-numerical "
                               "junk: '%s'", str, tail);
                else
                    *valuePP = xmlrpc_int_new(envP, i);
            }
        }
    }
}



static void
parseBoolean(xmlrpc_env *    const envP,
             const char *    const str,
             xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <boolean> XML-RPC XML element, e.g. "1".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (xmlrpc_streq(str, "0") || xmlrpc_streq(str, "1"))
        *valuePP = xmlrpc_bool_new(envP, xmlrpc_streq(str, "1") ? 1 : 0);
    else
        parseFault(envP, "<boolean> XML element content must be either "
                   "'0' or '1' according to XML-RPC.  This one has '%s'",
                   str);
}



static void
parseDouble(xmlrpc_env *    const envP,
            const char *    const str,
            xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <double> XML-RPC XML element, e.g. "34.5".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (str[0] == '\0')
        parseFault(envP, "<double> XML element content is empty.");
    else if (isspace(str[0]))
        parseFault(envP, "<double> content '%s' starts with white space.",
                   str);
    else {
        double d;
        char * tail;

        errno = 0;

        d = strtod(str, &tail);
    
        if (errno == ERANGE)
            parseFault(envP, "<double> XML element value '%s' represents a "
                       "number beyond the range that "
                       "XML-RPC allows", str);
        else if (errno != 0)
            parseFault(envP, "unexpected error parsing <double> XML element "
                       "value '%s'.  strtod() failed with errno %d (%s)",
                       str, errno, strerror(errno));
        else {
            if (tail[0] != '\0')
                parseFault(envP, "<double> value '%s' contains non-numerical "
                           "junk: '%s'", str, tail);
            else
                *valuePP = xmlrpc_double_new(envP, d);
        }
    }            
}



static void
parseBase64(xmlrpc_env *    const envP,
            const char *    const str,
            size_t          const strLength,
            xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <base64> XML-RPC XML element, e.g. "FD32YY".

   'str' is that content.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * decoded;
    
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    decoded = xmlrpc_base64_decode(envP, str, strLength);
    if (!envP->fault_occurred) {
        unsigned char * const bytes =
            XMLRPC_MEMBLOCK_CONTENTS(unsigned char, decoded);
        size_t const byteCount =
            XMLRPC_MEMBLOCK_SIZE(unsigned char, decoded);

        *valuePP = xmlrpc_base64_new(envP, byteCount, bytes);
        
        XMLRPC_MEMBLOCK_FREE(unsigned char, decoded);
    }
}



/* Forward declaration for recursion */
static xmlrpc_value *
convert_value(xmlrpc_env *  const envP,
              unsigned int  const maxRecursion,
              xml_element * const elemP);



/*=========================================================================
**  convert_array
**=========================================================================
**  Convert an XML element representing an array into an xmlrpc_value.
*/

static xmlrpc_value *
convert_array(xmlrpc_env *  const envP,
              unsigned int  const maxRecursion,
              xml_element * const elemP) {

    xml_element *data, **values, *value;
    xmlrpc_value *array, *item;
    int size, i;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Set up our error-handling preconditions. */
    array = item = NULL;

    /* Allocate an array to hold our values. */
    array = xmlrpc_build_value(envP, "()");
    XMLRPC_FAIL_IF_FAULT(envP);

    /* We don't need to check our element name--our callers do that. */
    CHECK_CHILD_COUNT(envP, elemP, 1);
    data = xml_element_children(elemP)[0];
    CHECK_NAME(envP, data, "data");
    
    /* Iterate over our children. */
    values = xml_element_children(data);
    size = xml_element_children_size(data);
    for (i = 0; i < size; i++) {
        value = values[i];
        item = convert_value(envP, maxRecursion-1, value);
        XMLRPC_FAIL_IF_FAULT(envP);

        xmlrpc_array_append_item(envP, array, item);
        xmlrpc_DECREF(item);
        item = NULL;
        XMLRPC_FAIL_IF_FAULT(envP);
    }

 cleanup:
    if (item)
        xmlrpc_DECREF(item);
    if (envP->fault_occurred) {
        if (array)
            xmlrpc_DECREF(array);
        return NULL;
    }
    return array;
}



/*=========================================================================
**  convert_struct
**=========================================================================
**  Convert an XML element representing a struct into an xmlrpc_value.
*/

static xmlrpc_value *
convert_struct(xmlrpc_env *  const envP,
               unsigned int  const maxRecursion,
               xml_element * const elemP) {

    xmlrpc_value *strct, *key, *value;
    xml_element **members, *member, *name_elemP, *value_elemP;
    int size, i;
    char *cdata;
    size_t cdata_size;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Set up our error-handling preconditions. */
    strct = key = value = NULL;

    /* Allocate an array to hold our members. */
    strct = xmlrpc_struct_new(envP);
    XMLRPC_FAIL_IF_FAULT(envP);

    /* Iterate over our children, extracting key/value pairs. */
    /* We don't need to check our element name--our callers do that. */
    members = xml_element_children(elemP);
    size = xml_element_children_size(elemP);
    for (i = 0; i < size; i++) {
        member = members[i];
        CHECK_NAME(envP, member, "member");
        CHECK_CHILD_COUNT(envP, member, 2);

        /* Get our key. */
        name_elemP = get_child_by_name(envP, member, "name");
        XMLRPC_FAIL_IF_FAULT(envP);
        CHECK_CHILD_COUNT(envP, name_elemP, 0);
        cdata = xml_element_cdata(name_elemP);
        cdata_size = xml_element_cdata_size(name_elemP);
        key = xmlrpc_string_new_lp(envP, cdata_size, cdata);
        XMLRPC_FAIL_IF_FAULT(envP);

        /* Get our value. */
        value_elemP = get_child_by_name(envP, member, "value");
        XMLRPC_FAIL_IF_FAULT(envP);
        value = convert_value(envP, maxRecursion-1, value_elemP);
        XMLRPC_FAIL_IF_FAULT(envP);

        /* Add the key/value pair to our struct. */
        xmlrpc_struct_set_value_v(envP, strct, key, value);
        XMLRPC_FAIL_IF_FAULT(envP);

        /* Release our references & memory, and restore our invariants. */
        xmlrpc_DECREF(key);
        key = NULL;
        xmlrpc_DECREF(value);
        value = NULL;
    }
    
 cleanup:
    if (key)
        xmlrpc_DECREF(key);
    if (value)
        xmlrpc_DECREF(value);
    if (envP->fault_occurred) {
        if (strct)
            xmlrpc_DECREF(strct);
        return NULL;
    }
    return strct;
}



static void
parseI8(xmlrpc_env *    const envP,
        const char *    const str,
        xmlrpc_value ** const valuePP) {
/*----------------------------------------------------------------------------
   Parse the content of a <i8> XML-RPC XML element, e.g. "34".

   'str' is that content.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_PTR_OK(str);

    if (str[0] == '\0')
        parseFault(envP, "<int> XML element content is empty.");
    else if (isspace(str[0]))
        parseFault(envP, "<int> content '%s' starts with white space.", str);
    else {
        long long i;
        char * tail;

        errno = 0;
        i = strtoll(str, &tail, 10);

        if (errno == ERANGE)
            parseFault(envP, "<int> XML element value '%s' represents a "
                       "number beyond the range that "
                       "XML-RPC allows (%d - %d)", str,
                       XMLRPC_INT64_MIN, XMLRPC_INT64_MAX);
        else if (errno != 0)
            parseFault(envP, "unexpected error parsing <int> XML element "
                       "value '%s'.  strtoll() failed with errno %d (%s)",
                       str, errno, strerror(errno));
        else {
            /* Look for out-of-range errors which didn't produce ERANGE. */
            if (i < XMLRPC_INT64_MIN)
                parseFault(envP, "<int> value %d is below the range allowed "
                           "by XML-RPC (minimum is %d)",
                           i, XMLRPC_INT64_MIN);
            else if (i > XMLRPC_INT64_MAX)
                parseFault(envP, "<int> value %d is above the range allowed "
                           "by XML-RPC (maximum is %d)",
                           i, XMLRPC_INT64_MAX);
            else {
                if (tail[0] != '\0')
                    parseFault(envP, "<int> value '%s' contains non-numerical "
                               "junk: '%s'", str, tail);
                else
                    *valuePP = xmlrpc_i8_new(envP, i);
            }
        }
    }
}



static void
parseSimpleValue(xmlrpc_env *    const envP,
                 const char *    const elementName,
                 const char *    const cdata,
                 size_t          const cdataLength,
                 xmlrpc_value ** const valuePP) {

    /* We need to straighten out the whole character set / encoding thing
       some day.  What is 'cdata', and what should it be?  Does it have
       embedded NUL?  Some of the code here assumes it doesn't.  Is it
       text?

       The <string> parser assumes it's UTF 8 with embedded NULs.
       But the <int> parser will get terribly confused if there are any
       UTF-8 multibyte sequences or NUL characters.  So will most of the
       others.
    */
    
    if (xmlrpc_streq(elementName, "i4") ||
        xmlrpc_streq(elementName, "int"))
        parseInt(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "boolean"))
        parseBoolean(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "double"))
        parseDouble(envP, cdata, valuePP);
    else if (xmlrpc_streq(elementName, "dateTime.iso8601"))
        *valuePP = xmlrpc_datetime_new_str(envP, cdata);
    else if (xmlrpc_streq(elementName, "string"))
        *valuePP = xmlrpc_string_new_lp(envP, cdataLength, cdata);
    else if (xmlrpc_streq(elementName, "base64"))
        parseBase64(envP, cdata, cdataLength, valuePP);
    else if (xmlrpc_streq(elementName, "nil"))
        *valuePP = xmlrpc_nil_new(envP);
    else if (xmlrpc_streq(elementName, "i8"))
        parseI8(envP, cdata, valuePP);
    else
        parseFault(envP, "Unknown value type -- XML element is named "
                   "<%s>", elementName);
}



static xmlrpc_value *
convert_value(xmlrpc_env *  const envP,
              unsigned int  const maxRecursion,
              xml_element * const elemP) {
/*----------------------------------------------------------------------------
   Compute the xmlrpc_value represented by the XML <value> element 'elem'.
   Return that xmlrpc_value.

   We call convert_array() and convert_struct(), which may ultimately
   call us recursively.  Don't recurse any more than 'maxRecursion'
   times.
-----------------------------------------------------------------------------*/
    int child_count;
    xmlrpc_value * retval;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Error-handling preconditions.
    ** If we haven't changed any of these from their default state, we're
    ** allowed to tail-call xmlrpc_build_value. */
    retval = NULL;

    /* Assume we'll need to recurse, make sure we're allowed */
    if (maxRecursion < 1) 
        XMLRPC_FAIL(envP, XMLRPC_PARSE_ERROR,
                    "Nested data structure too deep.");

    /* Validate our structure, and see whether we have a child element. */
    CHECK_NAME(envP, elemP, "value");
    child_count = xml_element_children_size(elemP);

    if (child_count == 0) {
        /* We have no type element, so treat the value as a string. */
        char * const cdata      = xml_element_cdata(elemP);
        size_t const cdata_size = xml_element_cdata_size(elemP);
        retval = xmlrpc_string_new_lp(envP, cdata_size, cdata);
    } else {
        /* We should have a type tag inside our value tag. */
        xml_element * child;
        const char * child_name;
        
        CHECK_CHILD_COUNT(envP, elemP, 1);
        child = xml_element_children(elemP)[0];
        
        /* Parse our value-containing element. */
        child_name = xml_element_name(child);

        if (xmlrpc_streq(child_name, "struct")) {
            retval = convert_struct(envP, maxRecursion, child);
        } else if (xmlrpc_streq(child_name, "array")) {
            CHECK_CHILD_COUNT(envP, child, 1);
            retval = convert_array(envP, maxRecursion, child);
        } else {
            char * cdata;
            size_t cdata_size;

            CHECK_CHILD_COUNT(envP, child, 0);
            cdata = xml_element_cdata(child);
            cdata_size = xml_element_cdata_size(child);

            parseSimpleValue(envP, child_name, cdata, cdata_size, &retval);

            XMLRPC_FAIL_IF_FAULT(envP);
        }
    }

 cleanup:
    if (envP->fault_occurred) {
        if (retval)
            xmlrpc_DECREF(retval);
        retval = NULL;
    }
    return retval;
}



/*=========================================================================
**  convert_params
**=========================================================================
**  Convert an XML element representing a list of params into an
**  xmlrpc_value (of type array).
*/

static xmlrpc_value *
convert_params(xmlrpc_env *        const envP,
               const xml_element * const elemP) {
/*----------------------------------------------------------------------------
   Convert an XML element representing a list of parameters (i.e.  a
   <params> element) to an xmlrpc_value of type array.  Note that an
   array is normally represented in XML by a <value> element.  We use
   type xmlrpc_value to represent the parameter list just for convenience.
-----------------------------------------------------------------------------*/
    xmlrpc_value *array, *item;
    int size, i;
    xml_element **params, *param, *value;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(elemP != NULL);

    /* Set up our error-handling preconditions. */
    array = item = NULL;

    /* Allocate an array to hold our parameters. */
    array = xmlrpc_build_value(envP, "()");
    XMLRPC_FAIL_IF_FAULT(envP);

    /* We're responsible for checking our own element name. */
    CHECK_NAME(envP, elemP, "params");    

    /* Iterate over our children. */
    size = xml_element_children_size(elemP);
    params = xml_element_children(elemP);
    for (i = 0; i < size; ++i) {
        unsigned int const maxNest = xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID);

        param = params[i];
        CHECK_NAME(envP, param, "param");
        CHECK_CHILD_COUNT(envP, param, 1);

        value = xml_element_children(param)[0];
        item = convert_value(envP, maxNest, value);
        XMLRPC_FAIL_IF_FAULT(envP);

        xmlrpc_array_append_item(envP, array, item);
        xmlrpc_DECREF(item);
        item = NULL;
        XMLRPC_FAIL_IF_FAULT(envP);
    }

 cleanup:
    if (envP->fault_occurred) {
        if (array)
            xmlrpc_DECREF(array);
        if (item)
            xmlrpc_DECREF(item);
        return NULL;
    }
    return array;
}



static void
parseCallXml(xmlrpc_env *   const envP,
             const char *   const xmlData,
             size_t         const xmlLen,
             xml_element ** const callElemPP) {
/*----------------------------------------------------------------------------
   Parse the XML of an XML-RPC call.
-----------------------------------------------------------------------------*/
    xml_element * callElemP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);
    xml_parse(&env, xmlData, xmlLen, &callElemP);
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code, "Call is not valid XML.  %s",
            env.fault_string);
    else {
        if (!xmlrpc_streq(xml_element_name(callElemP), "methodCall"))
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_PARSE_ERROR,
                "XML-RPC call should be a <methodCall> element.  "
                "Instead, we have a <%s> element.",
                xml_element_name(callElemP));

        if (!envP->fault_occurred)
            *callElemPP = callElemP;

        if (envP->fault_occurred)
            xml_element_free(callElemP);
    }
    xmlrpc_env_clean(&env);
}



static void
parseMethodNameElement(xmlrpc_env *  const envP,
                       xml_element * const nameElemP,
                       const char ** const methodNameP) {
    
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(nameElemP), "methodName"));

    if (xml_element_children_size(nameElemP) > 0)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "A <methodName> element should not have children.  "
            "This one has %u of them.", xml_element_children_size(nameElemP));
    else {
        const char * const cdata = xml_element_cdata(nameElemP);

        xmlrpc_validate_utf8(envP, cdata, strlen(cdata));

        if (!envP->fault_occurred) {
            *methodNameP = strdup(cdata);
            if (*methodNameP == NULL)
                xmlrpc_faultf(envP,
                              "Could not allocate memory for method name");
        }
    }
}            



static void
parseCallChildren(xmlrpc_env *    const envP,
                  xml_element *   const callElemP,
                  const char **   const methodNameP,
                  xmlrpc_value ** const paramArrayPP ) {
/*----------------------------------------------------------------------------
  Parse the children of a <methodCall> XML element *callElemP.  They should
  be <methodName> and <params>.
-----------------------------------------------------------------------------*/
    size_t const callChildCount = xml_element_children_size(callElemP);

    xml_element * nameElemP;
        
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(callElemP), "methodCall"));
    
    nameElemP = get_child_by_name(envP, callElemP, "methodName");
    
    if (!envP->fault_occurred) {
        parseMethodNameElement(envP, nameElemP, methodNameP);
            
        if (!envP->fault_occurred) {
            /* Convert our parameters. */
            if (callChildCount > 1) {
                xml_element * paramsElemP;

                paramsElemP = get_child_by_name(envP, callElemP, "params");
                    
                if (!envP->fault_occurred)
                    *paramArrayPP = convert_params(envP, paramsElemP);
            } else {
                /* Workaround for Ruby XML-RPC and old versions of
                   xmlrpc-epi.  Future improvement: Instead of looking
                   at child count, we should just check for existence
                   of <params>.
                */
                *paramArrayPP = xmlrpc_array_new(envP);
            }
            if (!envP->fault_occurred) {
                if (callChildCount > 2)
                    xmlrpc_env_set_fault_formatted(
                        envP, XMLRPC_PARSE_ERROR,
                        "<methodCall> has extraneous children, other than "
                        "<methodName> and <params>.  Total child count = %u",
                        callChildCount);
                    
                if (envP->fault_occurred)
                    xmlrpc_DECREF(*paramArrayPP);
            }
            if (envP->fault_occurred)
                xmlrpc_strfree(*methodNameP);
        }
    }
}



void 
xmlrpc_parse_call(xmlrpc_env *    const envP,
                  const char *    const xmlData,
                  size_t          const xmlLen,
                  const char **   const methodNameP,
                  xmlrpc_value ** const paramArrayPP) {
/*----------------------------------------------------------------------------
  Given some XML text, attempt to parse it as an XML-RPC call.
  Return as *methodNameP the name of the method identified in the call
  and as *paramArrayPP the parameter list as an XML-RPC array.
  Caller must free() and xmlrpc_DECREF() these, respectively).
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);
    XMLRPC_ASSERT(methodNameP != NULL && paramArrayPP != NULL);

    /* SECURITY: Last-ditch attempt to make sure our content length is
       legal.  XXX - This check occurs too late to prevent an attacker
       from creating an enormous memory block, so you should try to
       enforce it *before* reading any data off the network.
     */
    if (xmlLen > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC request too large.  Max allowed is %u bytes",
            xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID));
    else {
        xml_element * callElemP;
        parseCallXml(envP, xmlData, xmlLen, &callElemP);
        if (!envP->fault_occurred) {
            parseCallChildren(envP, callElemP, methodNameP, paramArrayPP);

            xml_element_free(callElemP);
        }
    }
    if (envP->fault_occurred) {
        /* Should not be necessary, but for backward compatibility: */
        *methodNameP  = NULL;
        *paramArrayPP = NULL;
    }
}



static void
interpretFaultValue(xmlrpc_env *   const envP,
                    xmlrpc_value * const faultVP,
                    int *          const faultCodeP,
                    const char **  const faultStringP) {
                
    if (faultVP->_type != XMLRPC_TYPE_STRUCT)
        xmlrpc_env_set_fault(
            envP, XMLRPC_PARSE_ERROR,
            "<value> element of <fault> response contains is not "
            "of structure type");
    else {
        xmlrpc_value * faultCodeVP;
        xmlrpc_env fvEnv;

        xmlrpc_env_init(&fvEnv);

        xmlrpc_struct_read_value(&fvEnv, faultVP, "faultCode", &faultCodeVP);
        if (!fvEnv.fault_occurred) {
            xmlrpc_read_int(&fvEnv, faultCodeVP, faultCodeP);
            if (!fvEnv.fault_occurred) {
                xmlrpc_value * faultStringVP;

                xmlrpc_struct_read_value(&fvEnv, faultVP, "faultString",
                                         &faultStringVP);
                if (!fvEnv.fault_occurred) {
                    xmlrpc_read_string(&fvEnv, faultStringVP, faultStringP);
                    xmlrpc_DECREF(faultStringVP);
                }
            }
            xmlrpc_DECREF(faultCodeVP);
        }
        if (fvEnv.fault_occurred)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_PARSE_ERROR,
                "Invalid struct for <fault> value.  %s", fvEnv.fault_string);

        xmlrpc_env_clean(&fvEnv);
    }
}



static void
parseFaultElement(xmlrpc_env *        const envP,
                  const xml_element * const faultElement,
                  int *               const faultCodeP,
                  const char **       const faultStringP) {
                  
    unsigned int const maxRecursion =
        xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID);

    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(faultElement), "fault"));

    if (xml_element_children_size(faultElement) != 1)
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "<fault> element should have 1 child, but it has %u.",
            xml_element_children_size(faultElement));
    else {
        xml_element * const faultValueP =
            xml_element_children(faultElement)[0];

        xmlrpc_value * faultVP;

        faultVP = convert_value(envP, maxRecursion, faultValueP);
        
        if (!envP->fault_occurred) {
            interpretFaultValue(envP, faultVP, faultCodeP, faultStringP);

            xmlrpc_DECREF(faultVP);
        }
    }
}



static void
parseParamsElement(xmlrpc_env *        const envP,
                   const xml_element * const paramsElementP,
                   xmlrpc_value **     const resultPP) {

    xmlrpc_value * paramsVP;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(paramsElementP), "params"));

    paramsVP = convert_params(envP, paramsElementP);

    if (!envP->fault_occurred) {
        int arraySize;
        xmlrpc_env sizeEnv;

        XMLRPC_ASSERT_ARRAY_OK(paramsVP);
        
        xmlrpc_env_init(&sizeEnv);

        arraySize = xmlrpc_array_size(&sizeEnv, paramsVP);
        /* Since it's a valid array, as asserted above, can't fail */
        XMLRPC_ASSERT(!sizeEnv.fault_occurred);

        if (arraySize != 1)
            xmlrpc_env_set_fault_formatted(
                &env, XMLRPC_PARSE_ERROR,
                "Contains %d items.  It should have 1.",
                arraySize);
        else {
            xmlrpc_array_read_item(envP, paramsVP, 0, resultPP);
        }
        xmlrpc_DECREF(paramsVP);
        xmlrpc_env_clean(&sizeEnv);
    }
    if (env.fault_occurred)
        xmlrpc_env_set_fault_formatted(
            envP, env.fault_code,
            "Invalid <params> element.  %s", env.fault_string);

    xmlrpc_env_clean(&env);
}



static void
parseMethodResponseElt(xmlrpc_env *        const envP,
                       const xml_element * const methodResponseEltP,
                       xmlrpc_value **     const resultPP,
                       int *               const faultCodeP,
                       const char **       const faultStringP) {
    
    XMLRPC_ASSERT(xmlrpc_streq(xml_element_name(methodResponseEltP),
                               "methodResponse"));

    if (xml_element_children_size(methodResponseEltP) == 1) {
        xml_element * const child =
            xml_element_children(methodResponseEltP)[0];
        
        if (xmlrpc_streq(xml_element_name(child), "params")) {
            /* It's a successful response */
            parseParamsElement(envP, child, resultPP);
            *faultStringP = NULL;
        } else if (xmlrpc_streq(xml_element_name(child), "fault")) {
            /* It's a failure response */
            parseFaultElement(envP, child, faultCodeP, faultStringP);
        } else
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_PARSE_ERROR,
                "<methodResponse> must contain <params> or <fault>, "
                "but contains <%s>.", xml_element_name(child));
    } else
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_PARSE_ERROR,
            "<methodResponse> has %u children, should have 1.",
            xml_element_children_size(methodResponseEltP));
}



void
xmlrpc_parse_response2(xmlrpc_env *    const envP,
                       const char *    const xmlData,
                       size_t          const xmlDataLen,
                       xmlrpc_value ** const resultPP,
                       int *           const faultCodeP,
                       const char **   const faultStringP) {
/*----------------------------------------------------------------------------
  Given some XML text, attempt to parse it as an XML-RPC response.

  If the response is a regular, valid response, return a new reference
  to the appropriate value as *resultP and return NULL as
  *faultStringP and nothing as *faultCodeP.

  If the response is valid, but indicates a failure of the RPC, return the
  fault string in newly malloc'ed space as *faultStringP and the fault
  code as *faultCodeP and nothing as *resultP.

  If the XML text is not a valid response or something prevents us from
  parsing it, return a description of the error as *envP and nothing else.
-----------------------------------------------------------------------------*/
    xml_element * response;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);

    /* SECURITY: Last-ditch attempt to make sure our content length is legal.
    ** XXX - This check occurs too late to prevent an attacker from creating
    ** an enormous memory block, so you should try to enforce it
    ** *before* reading any data off the network. */
    if (xmlDataLen > xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID))
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_LIMIT_EXCEEDED_ERROR,
            "XML-RPC response too large.  Our limit is %u characters.  "
            "We got %u characters",
            xmlrpc_limit_get(XMLRPC_XML_SIZE_LIMIT_ID), xmlDataLen);
    else {
        xml_parse(envP, xmlData, xmlDataLen, &response);
        if (!envP->fault_occurred) {
            /* Pick apart and verify our structure. */
            if (xmlrpc_streq(xml_element_name(response), "methodResponse")) {
                parseMethodResponseElt(envP, response,
                                       resultPP, faultCodeP, faultStringP);
            } else
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_PARSE_ERROR,
                    "XML-RPC response must consist of a "
                    "<methodResponse> element.  This has a <%s> instead.",
                    xml_element_name(response));
            
            xml_element_free(response);
        }
    }
}



xmlrpc_value *
xmlrpc_parse_response(xmlrpc_env * const envP,
                      const char * const xmlData,
                      size_t       const xmlDataLen) {
/*----------------------------------------------------------------------------
   This exists for backward compatibility.  It is like
   xmlrpc_parse_response2(), except that it merges the concepts of a
   failed RPC and an error in executing the RPC.
-----------------------------------------------------------------------------*/
    xmlrpc_value * retval;
    xmlrpc_value * result;
    const char * faultString;
    int faultCode;

    xmlrpc_parse_response2(envP, xmlData, xmlDataLen,
                           &result, &faultCode, &faultString);
    
    if (envP->fault_occurred)
        retval = NULL;
    else {
        if (faultString) {
            xmlrpc_env_set_fault(envP, faultCode, faultString);
            xmlrpc_strfree(faultString);
            retval = NULL;
        } else
            retval = result;  /* transfer reference */
    }
    return retval;
}



/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */

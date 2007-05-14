/*=============================================================================
                              xmlrpc_string
===============================================================================
  Routines for the "string" type of xmlrpc_value.

  By Bryan Henderson.

  Contributed to the public domain by its author.
=============================================================================*/

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "bool.h"
#include "mallocvar.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"



void
xmlrpc_destroyString(xmlrpc_value * const valueP) {

    if (valueP->_wcs_block)
        xmlrpc_mem_block_free(valueP->_wcs_block);

    xmlrpc_mem_block_clean(&valueP->_block);
}



static void
verifyNoNulls(xmlrpc_env * const envP,
              const char * const contents,
              unsigned int const len) {
/*----------------------------------------------------------------------------
   Verify that the character array 'contents', which is 'len' bytes long,
   does not contain any NUL characters, which means it can be made into
   a passable ASCIIZ string just by adding a terminating NUL.

   Fail if the array contains a NUL.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < len && !envP->fault_occurred; i++)
        if (contents[i] == '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, 
                "String must not contain NUL characters");
}



#if HAVE_UNICODE_WCHAR

static void
verifyNoNullsW(xmlrpc_env *    const envP,
               const wchar_t * const contents,
               unsigned int    const len) {
/*----------------------------------------------------------------------------
   Same as verifyNoNulls(), but for wide characters.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; i < len && !envP->fault_occurred; i++)
        if (contents[i] == '\0')
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_TYPE_ERROR, 
                "String must not contain NUL characters");
}
#endif



static void
validateStringType(xmlrpc_env *         const envP,
                   const xmlrpc_value * const valueP) {

    if (valueP->_type != XMLRPC_TYPE_STRING) {
        xmlrpc_env_set_fault_formatted(
            envP, XMLRPC_TYPE_ERROR, "Value of type %s supplied where "
            "string type was expected.", 
            xmlrpc_type_name(valueP->_type));
    }
}



static void
accessStringValue(xmlrpc_env *         const envP,
                  const xmlrpc_value * const valueP,
                  size_t *             const lengthP,
                  const char **        const contentsP) {
    
    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
        unsigned int const len = size - 1;
            /* The memblock has a null character added to the end */

        verifyNoNulls(envP, contents, len);

        *lengthP = len;
        *contentsP = contents;
    }
}
             


void
xmlrpc_read_string(xmlrpc_env *         const envP,
                   const xmlrpc_value * const valueP,
                   const char **        const stringValueP) {
/*----------------------------------------------------------------------------
   Read the value of an XML-RPC string as an ASCIIZ string.

   Return the string in newly malloc'ed storage that Caller must free.

   Fail if the string contains null characters (which means it wasn't
   really a string, but XML-RPC doesn't seem to understand what a string
   is, and such values are possible).
-----------------------------------------------------------------------------*/
    size_t length;
    const char * contents;

    accessStringValue(envP, valueP, &length, &contents);

    if (!envP->fault_occurred) {
        char * stringValue;
            
        stringValue = malloc(length+1);
        if (stringValue == NULL)
            xmlrpc_faultf(envP, "Unable to allocate space "
                          "for %u-character string", length);
        else {
            memcpy(stringValue, contents, length);
            stringValue[length] = '\0';

            *stringValueP = stringValue;
        }
    }
}



void
xmlrpc_read_string_old(xmlrpc_env *         const envP,
                       const xmlrpc_value * const valueP,
                       const char **        const stringValueP) {

    size_t length;
    accessStringValue(envP, valueP, &length, stringValueP);
}



void
xmlrpc_read_string_lp(xmlrpc_env *         const envP,
                      const xmlrpc_value * const valueP,
                      size_t *             const lengthP,
                      const char **        const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        unsigned int const size = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block);
        const char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);

        char * stringValue;

        stringValue = malloc(size);
        if (stringValue == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, "Unable to allocate %u bytes "
                "for string.", size);
        else {
            memcpy(stringValue, contents, size);
            *stringValueP = stringValue;
            *lengthP = size - 1;  /* Size includes terminating NUL */
        }
    }
}



void
xmlrpc_read_string_lp_old(xmlrpc_env *         const envP,
                          const xmlrpc_value * const valueP,
                          size_t *             const lengthP,
                          const char **        const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        *lengthP =      XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block) - 1;
        *stringValueP = XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
    }
}



static __inline__ void
setupWcsBlock(xmlrpc_env *   const envP,
              xmlrpc_value * const valueP) {
/*----------------------------------------------------------------------------
   Add a wcs block (wchar_t string) to the indicated xmlrpc_value if it
   doesn't have one already.
-----------------------------------------------------------------------------*/
    if (!valueP->_wcs_block) {
        char * const contents = 
            XMLRPC_MEMBLOCK_CONTENTS(char, &valueP->_block);
        size_t const len = 
            XMLRPC_MEMBLOCK_SIZE(char, &valueP->_block) - 1;
        valueP->_wcs_block = 
            xmlrpc_utf8_to_wcs(envP, contents, len + 1);
    }
}



#if HAVE_UNICODE_WCHAR

static void
accessStringValueW(xmlrpc_env *     const envP,
                   xmlrpc_value *   const valueP,
                   size_t *         const lengthP,
                   const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const len = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block) - 1;
            
            verifyNoNullsW(envP, wcontents, len);

            *lengthP = len;
            *stringValueP = wcontents;
        }
    }
}


              
void
xmlrpc_read_string_w(xmlrpc_env *     const envP,
                     xmlrpc_value *   const valueP,
                     const wchar_t ** const stringValueP) {

    size_t length;
    const wchar_t * wcontents;
    
    accessStringValueW(envP, valueP, &length, &wcontents);

    if (!envP->fault_occurred) {
        wchar_t * stringValue;
        stringValue = malloc((length + 1) * sizeof(wchar_t));
        if (stringValue == NULL)
            xmlrpc_env_set_fault_formatted(
                envP, XMLRPC_INTERNAL_ERROR, 
                "Unable to allocate space for %u-byte string", 
                length);
        else {
            memcpy(stringValue, wcontents, length * sizeof(wchar_t));
            stringValue[length] = '\0';
            
            *stringValueP = stringValue;
        }
    }
}



void
xmlrpc_read_string_w_old(xmlrpc_env *     const envP,
                         xmlrpc_value *   const valueP,
                         const wchar_t ** const stringValueP) {

    size_t length;

    accessStringValueW(envP, valueP, &length, stringValueP);
}



void
xmlrpc_read_string_w_lp(xmlrpc_env *     const envP,
                        xmlrpc_value *   const valueP,
                        size_t *         const lengthP,
                        const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);

            wchar_t * stringValue;
            
            stringValue = malloc(size * sizeof(wchar_t));
            if (stringValue == NULL)
                xmlrpc_env_set_fault_formatted(
                    envP, XMLRPC_INTERNAL_ERROR, 
                    "Unable to allocate space for %u-byte string", 
                    size);
            else {
                memcpy(stringValue, wcontents, size * sizeof(wchar_t));
                
                *lengthP      = size - 1; /* size includes terminating NUL */
                *stringValueP = stringValue;
            }
        }
    }
}



void
xmlrpc_read_string_w_lp_old(xmlrpc_env *     const envP,
                            xmlrpc_value *   const valueP,
                            size_t *         const lengthP,
                            const wchar_t ** const stringValueP) {

    validateStringType(envP, valueP);
    if (!envP->fault_occurred) {
        setupWcsBlock(envP, valueP);

        if (!envP->fault_occurred) {
            wchar_t * const wcontents = 
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valueP->_wcs_block);
            size_t const size = 
                XMLRPC_MEMBLOCK_SIZE(wchar_t, valueP->_wcs_block);
            
            *lengthP      = size - 1;  /* size includes terminatnig NUL */
            *stringValueP = wcontents;
        }
    }
}
#endif



static void
validateUtf(xmlrpc_env * const envP,
            const char * const value,
            size_t       const length) {

#if HAVE_UNICODE_WCHAR
    xmlrpc_validate_utf8(envP, value, length);
#endif
}



xmlrpc_value *
xmlrpc_string_new_lp(xmlrpc_env * const envP, 
                     size_t       const length,
                     const char * const value) {

    xmlrpc_value * valP;

    validateUtf(envP, value, length);

    if (!envP->fault_occurred) {
        xmlrpc_createXmlrpcValue(envP, &valP);

        if (!envP->fault_occurred) {
            valP->_type = XMLRPC_TYPE_STRING;
            valP->_wcs_block = NULL;
            XMLRPC_MEMBLOCK_INIT(char, envP, &valP->_block, length + 1);
            if (!envP->fault_occurred) {
                char * const contents =
                    XMLRPC_MEMBLOCK_CONTENTS(char, &valP->_block);
                memcpy(contents, value, length);
                contents[length] = '\0';
            }
            if (envP->fault_occurred)
                free(valP);
        }
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_new(xmlrpc_env * const envP,
                  const char * const value) {

    return xmlrpc_string_new_lp(envP, strlen(value), value);
}



xmlrpc_value *
xmlrpc_string_new_va(xmlrpc_env * const envP,
                     const char * const format,
                     va_list            args) {

    const char * formattedString;
    xmlrpc_value * retvalP;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(format != NULL);
    
    xmlrpc_vasprintf(&formattedString, format, args);

    if (formattedString == xmlrpc_strsol) {
        xmlrpc_faultf(envP, "Out of memory building formatted string");
        retvalP = NULL;  /* defeat compiler warning */
    } else
        retvalP = xmlrpc_string_new(envP, formattedString);

    xmlrpc_strfree(formattedString);

    return retvalP;
}



xmlrpc_value *
xmlrpc_string_new_f(xmlrpc_env * const envP,
                    const char * const format,
                    ...) {

    va_list args;
    xmlrpc_value * retval;
    
    va_start(args, format);
    
    retval = xmlrpc_string_new_va(envP, format, args);
    
    va_end(args);

    return retval;
}



#if HAVE_UNICODE_WCHAR
xmlrpc_value *
xmlrpc_string_w_new_lp(xmlrpc_env *    const envP, 
                       size_t          const length,
                       const wchar_t * const value) {

    xmlrpc_value * valP;

    /* Initialize our XML-RPC value. */
    xmlrpc_createXmlrpcValue(envP, &valP);

    if (!envP->fault_occurred) {
        valP->_type = XMLRPC_TYPE_STRING;

        /* Build our wchar_t block first. */
        valP->_wcs_block =
            XMLRPC_MEMBLOCK_NEW(wchar_t, envP, length + 1);
        if (!envP->fault_occurred) {
            wchar_t * const wcs_contents =
                XMLRPC_MEMBLOCK_CONTENTS(wchar_t, valP->_wcs_block);

            xmlrpc_mem_block * utf8P;

            memcpy(wcs_contents, value, length * sizeof(wchar_t));
            wcs_contents[length] = '\0';
    
            /* Convert the wcs block to UTF-8. */
            utf8P = xmlrpc_wcs_to_utf8(envP, wcs_contents, length + 1);
            if (!envP->fault_occurred) {
                char * const utf8_contents =
                    XMLRPC_MEMBLOCK_CONTENTS(char, utf8P);
                size_t const utf8_len = XMLRPC_MEMBLOCK_SIZE(char, utf8P);

                /* XXX - We need an extra memcopy to initialize _block. */
                XMLRPC_MEMBLOCK_INIT(char, envP, &valP->_block, utf8_len);
                if (!envP->fault_occurred) {
                    char * contents;
                    contents = XMLRPC_MEMBLOCK_CONTENTS(char, &valP->_block);
                    memcpy(contents, utf8_contents, utf8_len);
                }
                XMLRPC_MEMBLOCK_FREE(char, utf8P);
            }
            if (envP->fault_occurred)
                XMLRPC_MEMBLOCK_FREE(wchar_t, valP->_wcs_block);
        }
        if (envP->fault_occurred)
            free(valP);
    }
    return valP;
}



xmlrpc_value *
xmlrpc_string_w_new(xmlrpc_env *    const envP,
                    const wchar_t * const value) {
    return xmlrpc_string_w_new_lp(envP, wcslen(value), value);
}
#endif


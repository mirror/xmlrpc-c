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


/*============================================================================
                              xmlrpc_server_validatee
==============================================================================

  This program runs an XMLRPC server, using the Xmlrpc-c libraries.

  The server implements the methods that the Userland Validator1 test suite
  invokes, which are supposed to exercise a broad range of XMLRPC server
  function.

  Coments here used to say you could get information about Validator1
  from <http://validator.xmlrpc.com/>, but as of 2004.09.25, there's nothing
  there (there's a web server, but it is not configured to serve that
  particular URL).

============================================================================*/

#include <stdio.h>
#include <stdlib.h>

#include <xmlrpc.h>
#include <xmlrpc_server.h>
#include <xmlrpc_server_abyss.h>

#include "config.h"  /* information about this build environment */

#define RETURN_IF_FAULT(env) \
    do { \
        if ((env)->fault_occurred) \
            return NULL; \
    } while (0)
        

/*=========================================================================
**  validator1.arrayOfStructsTest
**=========================================================================
*/

static xmlrpc_value *
array_of_structs(xmlrpc_env *   const env, 
                 xmlrpc_value * const param_array, 
                 void *         const user_data ATTR_UNUSED) {

    xmlrpc_value *array, *strct;
    size_t size, i;
    xmlrpc_int32 curly, sum;

    /* Parse our argument array. */
    xmlrpc_parse_value(env, param_array, "(A)", &array);
    RETURN_IF_FAULT(env);

    /* Add up all the struct elements named "curly". */
    sum = 0;
    size = xmlrpc_array_size(env, array);
    RETURN_IF_FAULT(env);
    for (i = 0; i < size; i++) {
	strct = xmlrpc_array_get_item(env, array, i);
	RETURN_IF_FAULT(env);
	xmlrpc_parse_value(env, strct, "{s:i,*}", "curly", &curly);
	RETURN_IF_FAULT(env);
	sum += curly;
    }

    /* Return our result. */
    return xmlrpc_build_value(env, "i", sum);
}


/*=========================================================================
**  validator1.countTheEntities
**=========================================================================
*/

static xmlrpc_value *
count_entities(xmlrpc_env *   const env, 
               xmlrpc_value * const param_array, 
               void *         const user_data ATTR_UNUSED) {
    char *str;
    size_t len, i;
    xmlrpc_int32 left, right, amp, apos, quote;

    xmlrpc_parse_value(env, param_array, "(s#)", &str, &len);
    RETURN_IF_FAULT(env);

    left = right = amp = apos = quote = 0;
    for (i = 0; i < len; i++) {
	switch (str[i]) {
	    case '<': left++; break;
	    case '>': right++; break;
	    case '&': amp++; break;
	    case '\'': apos++; break;
	    case '\"': quote++; break;
	    default: break;
	}
    }

    return xmlrpc_build_value(env, "{s:i,s:i,s:i,s:i,s:i}",
			      "ctLeftAngleBrackets", left,
			      "ctRightAngleBrackets", right,
			      "ctAmpersands", amp,
			      "ctApostrophes", apos,
			      "ctQuotes", quote);
}


/*=========================================================================
**  validator1.easyStructTest
**=========================================================================
*/

static xmlrpc_value *
easy_struct(xmlrpc_env *   const env, 
            xmlrpc_value * const param_array,
            void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 larry, moe, curly;

    /* Parse our argument array and get the stooges. */
    xmlrpc_parse_value(env, param_array, "({s:i,s:i,s:i,*})",
		       "larry", &larry,
		       "moe", &moe,
		       "curly", &curly);
    RETURN_IF_FAULT(env);

    /* Return our result. */
    return xmlrpc_build_value(env, "i", larry + moe + curly);
}


/*=========================================================================
**  validator1.echoStructTest
**=========================================================================
*/

static xmlrpc_value *
echo_struct(xmlrpc_env *   const env, 
            xmlrpc_value * const param_array, 
            void *         const user_data ATTR_UNUSED) {
    xmlrpc_value *s;

    /* Parse our argument array. */
    xmlrpc_parse_value(env, param_array, "(S)", &s);
    RETURN_IF_FAULT(env);

    /* Create a new reference to the struct, and return it. */
    xmlrpc_INCREF(s);
    return s;
}


/*=========================================================================
**  validator1.manyTypesTest
**=========================================================================
*/

static xmlrpc_value *
many_types(xmlrpc_env *   const env ATTR_UNUSED, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

    /* Create another reference to our argument array and return it as is. */
    xmlrpc_INCREF(param_array);
    return param_array;
}


/*=========================================================================
**  validator1.moderateSizeArrayCheck
**=========================================================================
*/

static xmlrpc_value *
moderate_array(xmlrpc_env *   const env, 
               xmlrpc_value * const param_array, 
               void *         const user_data ATTR_UNUSED) {

    xmlrpc_value *array, *item, *result;
    int size;
    char *str1, *str2, *buffer;
    size_t str1_len, str2_len;

    /* Parse our argument array. */
    xmlrpc_parse_value(env, param_array, "(A)", &array);
    RETURN_IF_FAULT(env);

    /* Get the size of our array. */
    size = xmlrpc_array_size(env, array);
    RETURN_IF_FAULT(env);

    /* Get our first string. */
    item = xmlrpc_array_get_item(env, array, 0);
    RETURN_IF_FAULT(env);
    xmlrpc_parse_value(env, item, "s#", &str1, &str1_len);
    RETURN_IF_FAULT(env);

    /* Get our last string. */
    item = xmlrpc_array_get_item(env, array, size - 1);
    RETURN_IF_FAULT(env);
    xmlrpc_parse_value(env, item, "s#", &str2, &str2_len);
    RETURN_IF_FAULT(env);

    /* Concatenate the two strings. */
    buffer = (char*) malloc(str1_len + str2_len);
    if (!buffer) {
	xmlrpc_env_set_fault(env, 1, "Couldn't allocate concatenated string");
	return NULL;
    }
    memcpy(buffer, str1, str1_len);
    memcpy(&buffer[str1_len], str2, str2_len);

    /* Return our result (carefully). */
    result = xmlrpc_build_value(env, "s#", buffer, str1_len + str2_len);
    free(buffer);
    return result;
}


/*=========================================================================
**  validator1.nestedStructTest
**=========================================================================
*/

static xmlrpc_value *
nested_struct(xmlrpc_env *   const env, 
              xmlrpc_value * const param_array, 
              void *         const user_data ATTR_UNUSED) {

    xmlrpc_value *years;
    xmlrpc_int32 larry, moe, curly;

    /* Parse our argument array. */
    xmlrpc_parse_value(env, param_array, "(S)", &years);
    RETURN_IF_FAULT(env);

    /* Get values of larry, moe and curly for 2000-04-01. */
    xmlrpc_parse_value(env, years,
		       "{s:{s:{s:{s:i,s:i,s:i,*},*},*},*}",
		       "2000", "04", "01",
		       "larry", &larry,
		       "moe", &moe,
		       "curly", &curly);		       
    RETURN_IF_FAULT(env);

    /* Return our result. */
    return xmlrpc_build_value(env, "i", larry + moe + curly);
}


/*=========================================================================
**  validator1.simpleStructReturnTest
**=========================================================================
*/

static xmlrpc_value *
struct_return(xmlrpc_env *   const env, 
              xmlrpc_value * const param_array, 
              void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 i;

    xmlrpc_parse_value(env, param_array, "(i)", &i);
    RETURN_IF_FAULT(env);

    return xmlrpc_build_value(env, "{s:i,s:i,s:i}",
			      "times10", (xmlrpc_int32) i * 10,
			      "times100", (xmlrpc_int32) i * 100,
			      "times1000", (xmlrpc_int32) i * 1000);
}


/*=========================================================================
**  main
**=========================================================================
*/

int main(int           const argc, 
         const char ** const argv) {
    if (argc != 2) {
	fprintf(stderr, "Usage: validatee abyss.conf\n");
	exit(1);
    }

    xmlrpc_server_abyss_init(XMLRPC_SERVER_ABYSS_NO_FLAGS, argv[1]);

    xmlrpc_server_abyss_add_method("validator1.arrayOfStructsTest",
				   &array_of_structs, NULL);
    xmlrpc_server_abyss_add_method("validator1.countTheEntities",
				   &count_entities, NULL);
    xmlrpc_server_abyss_add_method("validator1.easyStructTest",
				   &easy_struct, NULL);
    xmlrpc_server_abyss_add_method("validator1.echoStructTest",
				   &echo_struct, NULL);
    xmlrpc_server_abyss_add_method("validator1.manyTypesTest",
				   &many_types, NULL);
    xmlrpc_server_abyss_add_method("validator1.moderateSizeArrayCheck",
				   &moderate_array, NULL);
    xmlrpc_server_abyss_add_method("validator1.nestedStructTest",
				   &nested_struct, NULL);
    xmlrpc_server_abyss_add_method("validator1.simpleStructReturnTest",
				   &struct_return, NULL);

    xmlrpc_server_abyss_run();

    /* This never gets executed. */
    return 0;
}

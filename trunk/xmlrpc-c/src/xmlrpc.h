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


#ifndef  _XMLRPC_H_
#define  _XMLRPC_H_ 1

#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*=========================================================================
**  Typedefs
**=========================================================================
**  We define names for these types, because they may change from platform
**  to platform.
*/

typedef signed int xmlrpc_int32;
typedef int xmlrpc_bool;


/*=========================================================================
**  Assertions and Debugging
**=========================================================================
**  We use xmlrpc_assert for internal sanity checks. For example:
**
**    xmlrpc_assert(ptr != NULL);
**
**  Assertions are only evaluated when debugging code is turned on. (To
**  turn debugging off, define NDEBUG.) Some rules for using assertions:
**
**    1) Assertions should never have side effects.
**    2) Assertions should never be used for run-time error checking.
**       Instead, they should be used to check for "can't happen" errors.
*/

#ifndef NDEBUG

#define XMLRPC_ASSERT(cond) \
    do \
        if (!(cond)) \
            xmlrpc_assertion_failed(__FILE__, __LINE__); \
    while (0)

#else
#define XMLRPC_ASSERT(cond) (0)
#endif

extern void xmlrpc_assertion_failed (char* file, int line);

/* Validate a pointer. */
#define XMLRPC_ASSERT_PTR_OK(ptr) \
    XMLRPC_ASSERT((ptr) != NULL)

/* We only call this if something truly drastic happens. */
#define XMLRPC_FATAL_ERROR(msg) xmlrpc_fatal_error(__FILE__, __LINE__, (msg))

extern void xmlrpc_fatal_error (char* file, int line, char* msg);

/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#ifdef XMLRPC_WANT_INTERNAL_DECLARATIONS
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)
#endif /* XMLRPC_WANT_INTERNAL_DECLARATIONS */


/*=========================================================================
**  xmlrpc_env
**=========================================================================
**  XML-RPC represents runtime errors as <fault> elements. These contain
**  <faultCode> and <faultString> elements.
**
**  Since we need as much thread-safety as possible, we borrow an idea from
**  CORBA--we store exception information in an "environment" object.
**  You'll pass this to many different functions, and it will get filled
**  out appropriately.
**
**  For example:
**
**    xmlrpc_env env;
**
**    xmlrpc_env_init(&env);
**
**    xmlrpc_do_something(&env);
**    if (env.fault_occurred)
**        report_error_appropriately();
**
**    xmlrpc_env_clean(&env);
*/

#define XMLRPC_INTERNAL_ERROR               (-500)
#define XMLRPC_TYPE_ERROR                   (-501)
#define XMLRPC_INDEX_ERROR                  (-502)
#define XMLRPC_PARSE_ERROR                  (-503)
#define XMLRPC_NETWORK_ERROR                (-504)
#define XMLRPC_TIMEOUT_ERROR                (-505)
#define XMLRPC_NO_SUCH_METHOD_ERROR         (-506)
#define XMLRPC_REQUEST_REFUSED_ERROR        (-507)
#define XMLRPC_INTROSPECTION_DISABLED_ERROR (-508)
#define XMLRPC_LIMIT_EXCEEDED_ERROR         (-509)
#define XMLRPC_INVALID_UTF8_ERROR           (-510)

typedef struct _xmlrpc_env {
    int   fault_occurred;
    xmlrpc_int32 fault_code;
    char* fault_string;
} xmlrpc_env;

/* Initialize and destroy the contents of the provided xmlrpc_env object.
** These functions will never fail. */
void xmlrpc_env_init (xmlrpc_env* env);
void xmlrpc_env_clean (xmlrpc_env* env);

/* Fill out an xmlrpc_fault with the specified values, and set the
** fault_occurred flag. This function will make a private copy of 'string',
** so you retain responsibility for your copy. */
void xmlrpc_env_set_fault (xmlrpc_env *env, int code, char *string);

/* The same as the above, but using a printf-style format string. */
void xmlrpc_env_set_fault_formatted (xmlrpc_env* env, int code,
				     char *format, ...);

/* A simple debugging assertion. */
#define XMLRPC_ASSERT_ENV_OK(env) \
    XMLRPC_ASSERT((env) != NULL && !(env)->fault_occurred)

/* This version must *not* interpret 'str' as a format string, to avoid
** several evil attacks. */
#define XMLRPC_FAIL(env,code,str) \
    do { xmlrpc_env_set_fault((env),(code),(str)); goto cleanup; } while (0)

#define XMLRPC_FAIL1(env,code,str,arg1) \
    do { \
        xmlrpc_env_set_fault_formatted((env),(code),(str),(arg1)); \
        goto cleanup; \
    } while (0)

#define XMLRPC_FAIL2(env,code,str,arg1,arg2) \
    do { \
        xmlrpc_env_set_fault_formatted((env),(code),(str),(arg1),(arg2)); \
        goto cleanup; \
    } while (0)

#define XMLRPC_FAIL3(env,code,str,arg1,arg2,arg3) \
    do { \
        xmlrpc_env_set_fault_formatted((env),(code), \
                                       (str),(arg1),(arg2),(arg3)); \
        goto cleanup; \
    } while (0)

#define XMLRPC_FAIL_IF_NULL(ptr,env,code,str) \
    do { \
        if ((ptr) == NULL) \
            XMLRPC_FAIL((env),(code),(str)); \
    } while (0)

#define XMLRPC_FAIL_IF_FAULT(env) \
    do { if ((env)->fault_occurred) goto cleanup; } while (0)


/*=========================================================================
**  Resource Limits
**=========================================================================
**  To discourage denial-of-service attacks, we provide several adjustable
**  resource limits. These functions are *not* re-entrant.
*/

/* Limit IDs. There will be more of these as time goes on. */
#define XMLRPC_NESTING_LIMIT_ID   (0)
#define XMLRPC_XML_SIZE_LIMIT_ID  (1)
#define XMLRPC_LAST_LIMIT_ID      (XMLRPC_XML_SIZE_LIMIT_ID)

/* By default, deserialized data may be no more than 64 levels deep. */
#define XMLRPC_NESTING_LIMIT_DEFAULT  (64)

/* By default, XML data from the network may be no larger than 512K.
** Some client and server modules may fail to enforce this properly. */
#define XMLRPC_XML_SIZE_LIMIT_DEFAULT (512*1024)

/* Set a specific limit to the specified value. */
extern void xmlrpc_limit_set (int limit_id, size_t value);

/* Get the value of a specified limit. */
extern size_t xmlrpc_limit_get (int limit_id);


/*=========================================================================
**  xmlrpc_mem_block
**=========================================================================
**  A resizable chunk of memory. This is mostly used internally, but it is
**  also used by the public API in a few places.
**  The struct fields are private!
*/

typedef struct _xmlrpc_mem_block {
    size_t _size;
    size_t _allocated;
    void*  _block;
} xmlrpc_mem_block;

/* Allocate a new xmlrpc_mem_block. */
xmlrpc_mem_block* xmlrpc_mem_block_new (xmlrpc_env* env, size_t size);

/* Destroy an existing xmlrpc_mem_block, and everything it contains. */
void xmlrpc_mem_block_free (xmlrpc_mem_block* block);

/* Initialize the contents of the provided xmlrpc_mem_block. */
void xmlrpc_mem_block_init
    (xmlrpc_env* env, xmlrpc_mem_block* block, size_t size);

/* Deallocate the contents of the provided xmlrpc_mem_block, but not the
** block itself. */
void xmlrpc_mem_block_clean (xmlrpc_mem_block* block);

/* Get the size and contents of the xmlrpc_mem_block. */
size_t xmlrpc_mem_block_size (xmlrpc_mem_block* block);
void* xmlrpc_mem_block_contents (xmlrpc_mem_block* block);

/* Resize an xmlrpc_mem_block, preserving as much of the contents as
** possible. */
void xmlrpc_mem_block_resize
    (xmlrpc_env* env, xmlrpc_mem_block* block, size_t size);

/* Append data to an existing xmlrpc_mem_block. */
void xmlrpc_mem_block_append
    (xmlrpc_env* env, xmlrpc_mem_block* block, void *data, size_t len);

/* A very lighweight "template" system for creating typed blocks of
** memory. This is not part of the normal public API, but users are
** welcome to use it if they wish. */
#define XMLRPC_TYPED_MEM_BLOCK_NEW(type,env,size) \
    xmlrpc_mem_block_new((env), sizeof(type) * (size))
#define XMLRPC_TYPED_MEM_BLOCK_FREE(type,block) \
    xmlrpc_mem_block_free(block)
#define XMLRPC_TYPED_MEM_BLOCK_INIT(type,env,block,size) \
    xmlrpc_mem_block_init((env), (block), sizeof(type) * (size))
#define XMLRPC_TYPED_MEM_BLOCK_CLEAN(type,block) \
    xmlrpc_mem_block_clean(block)
#define XMLRPC_TYPED_MEM_BLOCK_SIZE(type,block) \
    (xmlrpc_mem_block_size(block) / sizeof(type))
#define XMLRPC_TYPED_MEM_BLOCK_CONTENTS(type,block) \
    ((type*) xmlrpc_mem_block_contents(block))
#define XMLRPC_TYPED_MEM_BLOCK_RESIZE(type,env,block,size) \
    xmlrpc_mem_block_resize(env, block, sizeof(type) * (size))
#define XMLRPC_TYPED_MEM_BLOCK_APPEND(type,env,block,data,size) \
    xmlrpc_mem_block_append(env, block, data, sizeof(type) * (size))


/*=========================================================================
**  xmlrpc_value
**=========================================================================
**  An XML-RPC value (of any type).
*/

#define XMLRPC_TYPE_INT      (0)
#define XMLRPC_TYPE_BOOL     (1)
#define XMLRPC_TYPE_DOUBLE   (2)
#define XMLRPC_TYPE_DATETIME (3)
#define XMLRPC_TYPE_STRING   (4)
#define XMLRPC_TYPE_BASE64   (5)
#define XMLRPC_TYPE_ARRAY    (6)
#define XMLRPC_TYPE_STRUCT   (7)
#define XMLRPC_TYPE_C_PTR    (8)
#define XMLRPC_TYPE_DEAD     (0xDEAD)

typedef int xmlrpc_type;

#ifndef XMLRPC_WANT_INTERNAL_DECLARATIONS

/* These are *always* allocated on the heap. No exceptions. */
typedef struct _xmlrpc_value xmlrpc_value;

#else /* XMLRPC_WANT_INTERNAL_DECLARATIONS */

typedef struct _xmlrpc_value {
    xmlrpc_type _type;
    int _refcount;

    /* Certain data types store their data directly in the xmlrpc_value. */
    union {
	xmlrpc_int32 i;
	xmlrpc_bool b;
	double d;
	/* time_t t */
	void *c_ptr;
    } _value;

    /* Other data types use a memory block. */
    xmlrpc_mem_block _block;

    /* We may need to convert our string data to a wchar_t string. */
    xmlrpc_mem_block *_wcs_block;
} xmlrpc_value;

/* This is a private structure, but it's used in several different files. */
typedef struct {
    unsigned char key_hash;
    xmlrpc_value *key;
    xmlrpc_value *value;
} _struct_member;

#endif /* XMLRPC_WANT_INTERNAL_DECLARATIONS */


#define XMLRPC_ASSERT_VALUE_OK(val) \
    XMLRPC_ASSERT((val) != NULL && (val)->_type != XMLRPC_TYPE_DEAD)

/* A handy type-checking routine. */
#define XMLRPC_TYPE_CHECK(env,v,t) \
    do \
        if ((v)->_type != (t)) \
            XMLRPC_FAIL(env, XMLRPC_TYPE_ERROR, "Expected " #t); \
    while (0)

/* Increment the reference count of an xmlrpc_value. */
extern void xmlrpc_INCREF (xmlrpc_value* value);

/* Decrement the reference count of an xmlrpc_value. If there
** are no more references, free it. */
extern void xmlrpc_DECREF (xmlrpc_value* value);

/* Get the type of an XML-RPC value. */
extern xmlrpc_type xmlrpc_value_type (xmlrpc_value* value);

/* Build an xmlrpc_value from a format string.
** Increments the reference counts of input arguments if necessary.
** See the xmlrpc-c documentation for more information. */
extern xmlrpc_value *
xmlrpc_build_value (xmlrpc_env* env, char* format, ...);

/* The same as the above, but using a va_list. */
extern xmlrpc_value *
xmlrpc_build_value_va (xmlrpc_env* env, char* format, va_list args);

/* Extract values from an xmlrpc_value and store them into C variables.
** Does not increment the reference counts of output values.
** See the xmlrpc-c documentation for more information. */
extern void
xmlrpc_parse_value (xmlrpc_env* env, xmlrpc_value* value, char* format, ...);

/* Return the number of elements in an XML-RPC array.
** Sets XMLRPC_TYPE_ERROR if 'array' is not an array. */
extern int
xmlrpc_array_size (xmlrpc_env* env, xmlrpc_value* array);

/* Append an item to an XML-RPC array.
** Increments the reference count of 'value' if no fault occurs.
** Sets XMLRPC_TYPE_ERROR if 'array' is not an array. */
extern void
xmlrpc_array_append_item (xmlrpc_env* env,
			  xmlrpc_value* array,
			  xmlrpc_value* value);

/* Get an item from an XML-RPC array.
** Does not increment the reference count of the returned value.
** Sets XMLRPC_TYPE_ERROR if 'array' is not an array.
** Sets XMLRPC_INDEX_ERROR if 'index' is out of bounds. */
extern xmlrpc_value*
xmlrpc_array_get_item (xmlrpc_env* env,
		       xmlrpc_value* array,
		       int index);

/* Not implemented--we don't need it yet.
extern int xmlrpc_array_set_item (xmlrpc_env* env,
                                  xmlrpc_value* array,
                                  int index,
                                  xmlrpc_value* value);
*/

/* Create a new struct. */
extern xmlrpc_value *
xmlrpc_struct_new (xmlrpc_env* env);

/* Return the number of key/value pairs in a struct.
** Sets XMLRPC_TYPE_ERROR if 'strct' is not a struct. */
extern int
xmlrpc_struct_size (xmlrpc_env* env, xmlrpc_value* strct);

/* Returns true iff 'strct' contains 'key'.
** Sets XMLRPC_TYPE_ERROR if 'strct' is not a struct. */
extern int
xmlrpc_struct_has_key (xmlrpc_env* env, xmlrpc_value* strct, char* key);

/* The same as the above, but the key may contain zero bytes. */
extern int
xmlrpc_struct_has_key_n (xmlrpc_env* env, xmlrpc_value* strct,
			 char* key, size_t key_len);

/* Returns the value in 'strct' associated with 'key'.
** Does not increment the reference count of the returned value.
** Sets XMLRPC_TYPE_ERROR if 'strct' is not a struct.
** Sets XMLRPC_INDEX_ERROR if 'key' is not in 'strct'. */
extern xmlrpc_value*
xmlrpc_struct_get_value (xmlrpc_env* env, xmlrpc_value* strct, char* key);

/* The same as above, but the key may contain zero bytes. */
extern xmlrpc_value*
xmlrpc_struct_get_value_n (xmlrpc_env* env, xmlrpc_value* strct,
			   char* key, size_t key_len);

/* Set the value associated with 'key' in 'strct' to 'value'.
** Increments the reference count of value.
** Sets XMLRPC_TYPE_ERROR if 'strct' is not a struct. */
extern void
xmlrpc_struct_set_value (xmlrpc_env* env,
			 xmlrpc_value* strct,
			 char* key,
			 xmlrpc_value* value);

/* The same as above, but the key may contain zero bytes. */
extern void
xmlrpc_struct_set_value_n (xmlrpc_env* env,
			   xmlrpc_value* strct,
			   char* key, size_t key_len,
			   xmlrpc_value* value);

/* The same as above, but the key must be an XML-RPC string.
** Increments the reference count of keyval if it gets stored.
** Sets XMLRPC_TYPE_ERROR if 'keyval' is not a string. */
extern void
xmlrpc_struct_set_value_v (xmlrpc_env* env,
			   xmlrpc_value* strct,
			   xmlrpc_value* keyval,
			   xmlrpc_value* value);

/* Given a zero-based index, return the matching key and value. This
** is normally used in conjuction with xmlrpc_struct_size.
** Does not create new references to the returned key or value.
** Sets XMLRPC_TYPE_ERROR if 'strct' is not a struct.
** Sets XMLRPC_INDEX_ERROR if 'index' is out of bounds. */
extern void
xmlrpc_struct_get_key_and_value (xmlrpc_env *env,
				 xmlrpc_value *strct,
				 int index,
				 xmlrpc_value **out_keyval,
				 xmlrpc_value **out_value);


/*=========================================================================
**  Serialization
**=========================================================================
**  These routines convert xmlrpc_values into various kinds of strings.
**  You are responsible for allocating the 'output' block before you call
**  these functions, and deallocating it afterward.
**  You are, of course, responsible for deallocating any xmlrpc_values
**  which you pass to these functions.
*/

/* Serialize an XML value without any XML header. This is primarily used
** for testing purposes. */
extern void
xmlrpc_serialize_value (xmlrpc_env *env,
			xmlrpc_mem_block *output,
			xmlrpc_value *value);

/* Serialize a list of parameters without any XML header. This is
** primarily used for testing purposes. */
extern void
xmlrpc_serialize_params (xmlrpc_env *env,
			 xmlrpc_mem_block *output,
			 xmlrpc_value *param_array);

/* Serialize an XML-RPC call. */
extern void
xmlrpc_serialize_call (xmlrpc_env *env,
		       xmlrpc_mem_block *output,
		       char *method_name,
		       xmlrpc_value *param_array);

/* Serialize an XML-RPC return value. */
extern void
xmlrpc_serialize_response (xmlrpc_env *env,
			   xmlrpc_mem_block *output,
			   xmlrpc_value *value);

/* Serialize an XML-RPC fault (as specified by 'fault'). */
extern void
xmlrpc_serialize_fault (xmlrpc_env *env,
			xmlrpc_mem_block *output,
			xmlrpc_env *fault);


/*=========================================================================
**  Parsing
**=========================================================================
**  These routines convert XML into requests and responses.
*/

/* Parse an XML-RPC call. If an error occurs, set a fault and set
** the output variables to NULL.
** The caller is responsible for calling free(*out_method_name) and
** xmlrpc_DECREF(*out_param_array). */
extern void
xmlrpc_parse_call (xmlrpc_env *env,
		   char *xml_data,
		   size_t xml_len,
		   char **out_method_name,
		   xmlrpc_value **out_param_array);

/* Parse an XML-RPC response. If a fault occurs (or was received over the
** wire), return NULL and set up 'env'. The calling is responsible for
** calling xmlrpc_DECREF on the return value (if it isn't NULL). */
extern xmlrpc_value *
xmlrpc_parse_response (xmlrpc_env *env, char *xml_data, size_t xml_len);


/*=========================================================================
**  XML-RPC Server Method Registry
**=========================================================================
**  A method registry maintains a list of functions, and handles
**  dispatching. To build an XML-RPC server, just add a communications
**  protocol. :-)
**
**  Methods are C functions which take some combination of the following
**  parameters. All pointers except user_data belong to the library, and
**  must not be freed by the callback or used after the callback returns.
**
**  env:          An XML-RPC error-handling environment. No faults will be
**                set when the function is called. If an error occurs,
**                set an appropriate fault and return NULL. (If a fault is
**                set, the NULL return value will be enforced!)
**  host:         The 'Host:' header passed by the XML-RPC client, or NULL,
**                if no 'Host:' header has been provided.
**  method_name:  The name used to call this method.
**  user_data:    The user_data used to register this method.
**  param_array:  The parameters passed to this function, stored in an
**                XML-RPC array. You are *not* responsible for calling
**                xmlrpc_DECREF on this array.
**
**  Return value: If no fault has been set, the function must return a
**                valid xmlrpc_value. This will be serialized, returned
**                to the caller, and xmlrpc_DECREF'd.
*/

/* An ordinary method. */
typedef xmlrpc_value *
(*xmlrpc_method) (xmlrpc_env *env,
		  xmlrpc_value *param_array,
		  void *user_data);

/* A default method to call if no method can be found. */
typedef xmlrpc_value *
(*xmlrpc_default_method) (xmlrpc_env *env,
			  char *host,
			  char *method_name,
			  xmlrpc_value *param_array,
			  void *user_data);

#ifndef XMLRPC_WANT_INTERNAL_DECLARATIONS

/* Our registry structure. This has no public members. */
typedef struct _xmlrpc_registry xmlrpc_registry;

#else /* XMLRPC_WANT_INTERNAL_DECLARATIONS */

typedef struct _xmlrpc_registry {
    int _introspection_enabled;
    xmlrpc_value *_methods;
    xmlrpc_value *_default_method;
} xmlrpc_registry;

#endif /* XMLRPC_WANT_INTERNAL_DECLARATIONS */

/* Create a new method registry. */
extern xmlrpc_registry *
xmlrpc_registry_new (xmlrpc_env *env);

/* Delete a method registry. */
extern void
xmlrpc_registry_free (xmlrpc_registry *registry);

/* Disable introspection. By default, the xmlrpc_registry has built-in
** support for introspection. If like to make nosy people work harder,
** your can turn this off. */
extern void
xmlrpc_registry_disable_introspection(xmlrpc_registry *registry);

/* Register a method. The host parameter must be NULL (for now). You
** are responsible for owning and managing user_data. The registry
** will make internal copies of any other pointers it needs to
** keep around. */
extern void
xmlrpc_registry_add_method (xmlrpc_env *env,
			    xmlrpc_registry *registry,
			    char *host,
			    char *method_name,
			    xmlrpc_method method,
			    void *user_data);

/* As above, but allow the user to supply introspection information. 
**
** Signatures use their own little description language. It consists
** of one-letter type code (similar to the ones used in xmlrpc_parse_value)
** for the result, a colon, and zero or more one-letter type codes for
** the parameters. For example:
**   i:ibdsAS86
** If a function has more than one possible prototype, separate them with
** commas:
**   i:,i:s,i:ii
** If the function signature can't be represented using this language,
** pass a single question mark:
**   ?
** Help strings are ASCII text, and may contain HTML markup. */
extern void
xmlrpc_registry_add_method_w_doc (xmlrpc_env *env,
				  xmlrpc_registry *registry,
				  char *host,
				  char *method_name,
				  xmlrpc_method method,
				  void *user_data,
				  char *signature,
				  char *help);

/* Given a registry, a host name, and XML data; parse the <methodCall>,
** find the appropriate method, call it, serialize the response, and
** return it as an xmlrpc_mem_block. Most errors will be serialized
** as <fault> responses. If a *really* bad error occurs, set a fault and
** return NULL. (Actually, we currently give up with a fatal error,
** but that should change eventually.)
** The caller is responsible for destroying the memory block. */
extern xmlrpc_mem_block *
xmlrpc_registry_process_call (xmlrpc_env *env,
			      xmlrpc_registry *registry,
			      char *host,
			      char *xml_data,
			      size_t xml_len);

/* Define a default method for the specified registry.  This will be invoked
** if no other method matches.  The user_data pointer is property of the
** application, and will not be freed or manipulated by the registry. */
extern void
xmlrpc_registry_set_default_method (xmlrpc_env *env,
				    xmlrpc_registry *registry,
				    xmlrpc_default_method handler,
				    void *user_data);

				    
/*=========================================================================
**  XML-RPC Base64 Utilities
**=========================================================================
**  Here are some lightweight utilities which can be used to encode and
**  decode Base64 data. These are exported mainly for testing purposes.
*/

/* This routine inserts newlines every 76 characters, as required by the
** Base64 specification. */
extern xmlrpc_mem_block *
xmlrpc_base64_encode (xmlrpc_env *env,
		      unsigned char *bin_data,
		      size_t bin_len);

/* This routine encodes everything in one line. This is needed for HTTP
** authentication and similar tasks. */
extern xmlrpc_mem_block *
xmlrpc_base64_encode_without_newlines (xmlrpc_env *env,
				       unsigned char *bin_data,
				       size_t bin_len);

/* This decodes Base64 data with or without newlines. */
extern xmlrpc_mem_block *
xmlrpc_base64_decode (xmlrpc_env *env,
		      unsigned char *ascii_data,
		      size_t ascii_len);


/*=========================================================================
**  UTF-8 Encoding and Decoding
**=========================================================================
**  We need a correct, reliable and secure UTF-8 decoder. This decoder
**  raises a fault if it encounters invalid UTF-8.
**
**  Note that ANSI C does not precisely define the representation used
**  by wchar_t--it may be UCS-2, UTF-16, UCS-4, or something from outer
**  space. If your platform does something especially bizarre, you may
**  need to reimplement these routines.
*/

/* Ensure that a string contains valid, legally-encoded UTF-8 data.
** (Incorrectly-encoded UTF-8 strings are often used to bypass security
** checks.) */
extern void
xmlrpc_validate_utf8 (xmlrpc_env *env,
		      char *utf8_data,
		      size_t utf8_len);

/* Decode a UTF-8 string. */
extern xmlrpc_mem_block *
xmlrpc_utf8_to_wcs (xmlrpc_env *env,
		    char *utf8_data,
		    size_t utf8_len);

/* Encode a UTF-8 string. */
extern xmlrpc_mem_block *
xmlrpc_wcs_to_utf8 (xmlrpc_env *env,
		    wchar_t *wcs_data,
		    size_t wcs_len);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _XMLRPC_H_ */


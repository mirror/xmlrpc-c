/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
** Copyright (C) 2001 by Eric Kidd. All rights reserved.
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


#include "xmlrpc_config.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"

/* Borrowed from Python 1.5.2.
** MPW pushes 'extended' for float and double types with varargs */
#ifdef MPW
typedef extended va_double;
#else
typedef double va_double;
#endif

/* Borrowed from Python 1.5.2.
** Python copies its va_list objects before using them in certain
** tricky fashions. We don't why Python does this, but since we're
** abusing our va_list objects in a similar fashion, we'll copy them
** too. */
#ifdef VA_LIST_IS_ARRAY
#define VA_LIST_COPY(dest,src) memcpy((dest), (src), sizeof(va_list))
#else
#define VA_LIST_COPY(dest,src) ((dest) = (src))
#endif


/*=========================================================================
**  Reference Counting
**=========================================================================
**  Some simple reference-counting code. The xmlrpc_DECREF routine is in
**  charge of destroying values when their reference count equals zero.
*/

void xmlrpc_INCREF (xmlrpc_value* value)
{
    XMLRPC_ASSERT_VALUE_OK(value);
    XMLRPC_ASSERT(value->_refcount > 0);

    value->_refcount++;
}

void xmlrpc_DECREF (xmlrpc_value* value)
{
    xmlrpc_env env;
    int size, i;
    xmlrpc_value *item;
    _struct_member *members;

    XMLRPC_ASSERT_VALUE_OK(value);
    XMLRPC_ASSERT(value->_refcount > 0);
    XMLRPC_ASSERT(value->_type != XMLRPC_TYPE_DEAD);

    value->_refcount--;

    /* If we have no more refs, we need to deallocate this value. */
    if (value->_refcount == 0) {

	/* First, we need to destroy this value's contents, if any. */
	switch (value->_type) {
	    case XMLRPC_TYPE_INT:
	    case XMLRPC_TYPE_BOOL:
	    case XMLRPC_TYPE_DOUBLE:
		break;
		
	    case XMLRPC_TYPE_ARRAY:
		/* Dispose of the contents of the array.
		** No errors should *ever* occur when this code is running,
		** so we use assertions instead of regular error checks. */
		xmlrpc_env_init(&env);
		size = xmlrpc_array_size(&env, value);
		XMLRPC_ASSERT(!env.fault_occurred);
		for (i = 0; i < size; i++) {
		    item = xmlrpc_array_get_item(&env, value, i);
		    XMLRPC_ASSERT(!env.fault_occurred);
		    xmlrpc_DECREF(item);
		}
		xmlrpc_env_clean(&env);
		/* Fall through. */

	    case XMLRPC_TYPE_STRING:
	    case XMLRPC_TYPE_DATETIME:
	    case XMLRPC_TYPE_BASE64:
		xmlrpc_mem_block_clean(&value->_block);
		break;

	    case XMLRPC_TYPE_STRUCT:
		/* Dispose of the contents of the struct.
		** No errors should *ever* occur when this code is running,
		** so we use assertions instead of regular error checks. */
		size = XMLRPC_TYPED_MEM_BLOCK_SIZE(_struct_member,
						   &value->_block);
		members = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(_struct_member,
							  &value->_block);
		for (i = 0; i < size; i++) {
		    xmlrpc_DECREF(members[i].key);
		    xmlrpc_DECREF(members[i].value);
		}
		xmlrpc_mem_block_clean(&value->_block);
		break;


	    case XMLRPC_TYPE_C_PTR:
		break;

	    case XMLRPC_TYPE_DEAD:
		XMLRPC_FATAL_ERROR("Tried to destroy deallocated value");

	    default:
		XMLRPC_FATAL_ERROR("Unknown XML-RPC type");
	}

	/* Next, we mark this value as invalid, to help catch refcount
	** errors. */
	value->_type = XMLRPC_TYPE_DEAD;

	/* Finally, we destroy the value itself. */
	free(value);
    }
}


/*=========================================================================
**  Building XML-RPC values.
**=========================================================================
*/

xmlrpc_type xmlrpc_value_type (xmlrpc_value* value)
{
    XMLRPC_ASSERT_VALUE_OK(value);
    return value->_type;
}


/*=========================================================================
**  Building XML-RPC values.
**=========================================================================
**  Build new XML-RPC values from a format string. This code is heavily
**  inspired by Py_BuildValue from Python 1.5.2. In particular, our
**  particular abuse of the va_list data type is copied from the equivalent
**  Python code in modsupport.c. Since Python is portable, our code should
**  (in theory) also be portable.
*/

static xmlrpc_value* mkvalue(xmlrpc_env* env, char** format, va_list* args);

static xmlrpc_value* mkarray(xmlrpc_env* env,
			     char** format,
			     char delimiter,
			     va_list* args)
{
    xmlrpc_value *array, *item;
    int array_valid;
    char code;

    /* Set up error handling preconditions. */
    array = NULL;
    array_valid = 0;

    /* Allocate our array. */
    array = (xmlrpc_value*) malloc(sizeof(xmlrpc_value));
    XMLRPC_FAIL_IF_NULL(array, env, XMLRPC_INTERNAL_ERROR,
			"Could not allocate memory for array");
    array->_refcount = 1;
    array->_type = XMLRPC_TYPE_ARRAY;
    XMLRPC_TYPED_MEM_BLOCK_INIT(xmlrpc_value*, env, &array->_block, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    array_valid = 1;

    /* Add items to the array until we hit our delimiter. */
    code = **format;
    while (code != delimiter && code != '\0') {

	item = mkvalue(env, format, args);
	XMLRPC_FAIL_IF_FAULT(env);

	xmlrpc_array_append_item(env, array, item);
	xmlrpc_DECREF(item);
	XMLRPC_FAIL_IF_FAULT(env);

	code = **format;
    }
    XMLRPC_ASSERT(code == delimiter);

 cleanup:
    if (env->fault_occurred) {
	if (array) {
	    if (array_valid)
		xmlrpc_DECREF(array);
	    else
		free(array);
	}
	return NULL;
    }
    return array;
}

static xmlrpc_value* mkstruct(xmlrpc_env* env,
			      char** format,
			      char delimiter,
			      va_list* args)
{
    xmlrpc_value *strct, *key, *value;

    /* Set up error handling preconditions. */
    strct = key = value = NULL;

    /* Allocate a new struct for us to use. */
    strct = xmlrpc_struct_new(env);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Build the members of our struct. */
    while (**format != delimiter && **format != '\0') {

	/* Get our key, and skip over the ':' character. */
	key = mkvalue(env, format, args);
	XMLRPC_FAIL_IF_FAULT(env);
	XMLRPC_ASSERT(**format == ':');
	(*format)++;

	/* Get our value, and skip over the ',' character (if present). */
	value = mkvalue(env, format, args);
	XMLRPC_FAIL_IF_FAULT(env);
	XMLRPC_ASSERT(**format == ',' || **format == delimiter);
	if (**format == ',')
	    (*format)++;

	/* Add the new key/value pair to the struct. */
	xmlrpc_struct_set_value_v(env, strct, key, value);
	XMLRPC_FAIL_IF_FAULT(env);

	/* Release our references, and restore our invariants. */
	xmlrpc_DECREF(key);
	key = NULL;
	xmlrpc_DECREF(value);
	value = NULL;
    }
    XMLRPC_ASSERT(**format == delimiter);

 cleanup:
    if (env->fault_occurred) {
	if (strct)
	    xmlrpc_DECREF(strct);
	if (key)
	    xmlrpc_DECREF(key);
	if (value)
	    xmlrpc_DECREF(value);
	return NULL;
    }
    return strct;
}

static xmlrpc_value* mkvalue(xmlrpc_env* env, char** format, va_list* args)
{
    xmlrpc_value* val;
    char *str, *contents;
    unsigned char *bin_data;
    size_t len;

    /* Allocate some memory which we'll almost certainly use. If we don't
    ** use it, we'll deallocate it before returning. */
    val = (xmlrpc_value*) malloc(sizeof(xmlrpc_value));
    if (!val) {
	xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR,
			     "Could not allocate memory for xmlrpc_value");
	return NULL;
    }
    val->_refcount = 1;

    /* Process the next format character. */
    switch (*(*format)++) {
	case 'i':
	    val->_type = XMLRPC_TYPE_INT;
	    val->_value.i = (xmlrpc_int32) va_arg(*args, xmlrpc_int32);
	    break;

	case 'b':
	    val->_type = XMLRPC_TYPE_BOOL;
	    val->_value.b = (xmlrpc_bool) va_arg(*args, xmlrpc_bool);
	    break;

	case 'd':
	    val->_type = XMLRPC_TYPE_DOUBLE;
	    val->_value.d = (double) va_arg(*args, va_double);
	    break;

	case 's':
	    val->_type = XMLRPC_TYPE_STRING;
	    str = (char*) va_arg(*args, char*);
	    if (**format == '#') {
		(*format)++;
		len = (size_t) va_arg(*args, size_t);
	    } else {
		len = strlen(str);
	    }
	    XMLRPC_TYPED_MEM_BLOCK_INIT(char, env, &val->_block, len + 1);
	    XMLRPC_FAIL_IF_FAULT(env);
	    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &val->_block);
	    memcpy(contents, str, len);
	    contents[len] = '\0';
	    break;

	case '8':
	    /* The code 't' is reserved for a better, time_t based
	    ** implementation of dateTime conversion. */
	    val->_type = XMLRPC_TYPE_DATETIME;
	    str = (char*) va_arg(*args, char*);
	    len = strlen(str);
	    XMLRPC_TYPED_MEM_BLOCK_INIT(char, env, &val->_block, len + 1);
	    XMLRPC_FAIL_IF_FAULT(env);
	    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &val->_block);
	    memcpy(contents, str, len);
	    contents[len] = '\0';
	    break;

	case '6':
	    val->_type = XMLRPC_TYPE_BASE64;
	    bin_data = (unsigned char*) va_arg(*args, unsigned char*);
	    len = (size_t) va_arg(*args, size_t);
	    xmlrpc_mem_block_init(env, &val->_block, len);
	    XMLRPC_FAIL_IF_FAULT(env);
	    contents = xmlrpc_mem_block_contents(&val->_block);
	    memcpy(contents, bin_data, len);
	    break;

	case 'p':
	    /* We might someday want to use the code 'p!' to read in a
	    ** cleanup function for this pointer. */
	    val->_type = XMLRPC_TYPE_C_PTR;
	    val->_value.c_ptr = (void*) va_arg(*args, void*);
	    break;	    

	case 'V':
	    free(val); /* We won't need that after all, I guess. */
	    val = (xmlrpc_value*) va_arg(*args, xmlrpc_value*);
	    xmlrpc_INCREF(val);
	    break;

	case '(':
	    free(val); /* We won't need that after all, I guess. */
	    val = mkarray(env, format, ')', args);
	    XMLRPC_FAIL_IF_FAULT(env);
	    (*format)++;
	    break;

	case '{':
	    free(val); /* We won't need that after all, I guess. */
	    val = mkstruct(env, format, '}', args);
	    XMLRPC_FAIL_IF_FAULT(env);
	    (*format)++;
	    break;

	default:
	    XMLRPC_FATAL_ERROR("Unknown type code when building value");
    }

 cleanup:
    if (env->fault_occurred && val) {
	free(val);
	return NULL;
    }
    return val;
}

xmlrpc_value* xmlrpc_build_value_va (xmlrpc_env* env,
				     char* format,
				     va_list args)
{
    char *format_copy;
    va_list args_copy;
    xmlrpc_value* retval;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT(format != NULL);

    format_copy = format;
    VA_LIST_COPY(args_copy, args);
    retval = mkvalue(env, &format_copy, &args_copy);

    if (!env->fault_occurred) {
	XMLRPC_ASSERT_VALUE_OK(retval);
	XMLRPC_ASSERT(*format_copy == '\0');
    }

    return retval;    
}

xmlrpc_value* xmlrpc_build_value (xmlrpc_env* env,
				  char* format, ...)
{
    va_list args;
    xmlrpc_value* retval;

    va_start(args, format);
    retval = xmlrpc_build_value_va(env, format, args);
    va_end(args);

    return retval;
}


/*=========================================================================
**  Parsing XML-RPC values.
**=========================================================================
**  Parse an XML-RPC value based on a format string. This code is heavily
**  inspired by Py_BuildValue from Python 1.5.2.
*/

static void parsevalue (xmlrpc_env* env,
			xmlrpc_value* val,
			char** format,
			va_list* args);

static void parsearray (xmlrpc_env* env,
			xmlrpc_value* array,
			char** format,
			char delimiter,
			va_list* args)
{
    int size, i;
    xmlrpc_value *item;

    /* Fetch the array size. */
    size = xmlrpc_array_size(env, array);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Loop over the items in the array. */
    for (i = 0; i < size; i++) {
	/* Bail out if the caller didn't care about the rest of the items. */
	if (**format == '*')
	    break;

	item = xmlrpc_array_get_item(env, array, i);
	XMLRPC_FAIL_IF_FAULT(env);

	XMLRPC_ASSERT(**format != '\0');
	if (**format == delimiter)
	    XMLRPC_FAIL(env, XMLRPC_INDEX_ERROR, "Too many items in array");
	parsevalue(env, item, format, args);
	XMLRPC_FAIL_IF_FAULT(env);
    }
    if (**format == '*')
	(*format)++;
    if (**format != delimiter)
	XMLRPC_FAIL(env, XMLRPC_INDEX_ERROR, "Not enough items in array");

 cleanup:
}

static void parsestruct(xmlrpc_env* env,
			xmlrpc_value* strct,
			char** format,
			char delimiter,
			va_list* args)
{
    xmlrpc_value *key, *value;
    char *keystr;
    size_t keylen;

    /* Set up error handling preconditions. */
    key = NULL;

    /* Build the members of our struct. */
    while (**format != '*' && **format != delimiter && **format != '\0') {

	/* Get our key, and skip over the ':' character. Notice the
	** sudden call to mkvalue--we're going in the opposite direction. */
	key = mkvalue(env, format, args);
	XMLRPC_FAIL_IF_FAULT(env);
	XMLRPC_ASSERT(**format == ':');
	(*format)++;

	/* Look up the value for our key. */
	xmlrpc_parse_value(env, key, "s#", &keystr, &keylen);
	XMLRPC_FAIL_IF_FAULT(env);
	value = xmlrpc_struct_get_value_n(env, strct, keystr, keylen);
	XMLRPC_FAIL_IF_FAULT(env);

	/* Get our value, and skip over the ',' character (if present). */
	parsevalue(env, value, format, args);
	XMLRPC_FAIL_IF_FAULT(env);
	XMLRPC_ASSERT(**format == ',' || **format == delimiter);
	if (**format == ',')
	    (*format)++;

	/* Release our reference, and restore our invariant. */
	xmlrpc_DECREF(key);
	key = NULL;
    }
    XMLRPC_ASSERT(**format == '*');
    (*format)++;
    XMLRPC_ASSERT(**format == delimiter);

 cleanup:
    if (key)
	xmlrpc_DECREF(key);
}

static void parsevalue (xmlrpc_env* env,
			xmlrpc_value* val,
			char** format,
			va_list* args)
{
    xmlrpc_int32 *int32ptr;
    xmlrpc_bool *boolptr;
    double *doubleptr;
    char *contents;
    unsigned char *bin_data;
    char **strptr;
    void **voidptrptr;
    unsigned char **binptr;
    size_t len, i, *sizeptr;
    xmlrpc_value **valptr;

    switch (*(*format)++) {
	case 'i':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_INT);
	    int32ptr = (xmlrpc_int32*) va_arg(*args, xmlrpc_int32*);
	    *int32ptr = val->_value.i;
	    break;

	case 'b':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_BOOL);
	    boolptr = (xmlrpc_bool*) va_arg(*args, xmlrpc_bool*);
	    *boolptr = val->_value.b;	    
	    break;

	case 'd':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_DOUBLE);
	    doubleptr = (double*) va_arg(*args, double*);
	    *doubleptr = val->_value.d;	    
	    break;

	case 's':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_STRING);
	    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &val->_block);
	    len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &val->_block) - 1;

	    strptr = (char**) va_arg(*args, char**);
	    if (**format == '#') {
		(*format)++;
		sizeptr = (size_t*) va_arg(*args, size_t**);
		*sizeptr = len;
	    } else {
		for (i = 0; i < len; i++)
		    if (contents[i] == '\0')
			XMLRPC_FAIL(env, XMLRPC_TYPE_ERROR,
				    "String must not contain NULL characters");
	    }
	    *strptr = contents;
	    break;

	case '8':
	    /* The code 't' is reserved for a better, time_t based
	    ** implementation of dateTime conversion. */
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_DATETIME);
	    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &val->_block);
	    strptr = (char**) va_arg(*args, char**);
	    *strptr = contents;
	    break;

	case '6':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_BASE64);
	    bin_data = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(unsigned char,
						       &val->_block);
	    len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &val->_block);

	    binptr = (unsigned char**) va_arg(*args, unsigned char**);
	    *binptr = bin_data;
	    sizeptr = (size_t*) va_arg(*args, size_t**);	    
	    *sizeptr = len;
	    break;

	case 'p':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_C_PTR);
	    voidptrptr = (void**) va_arg(*args, void**);
	    *voidptrptr = val->_value.c_ptr;
	    break;

	case 'V':
	    valptr = (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
	    *valptr = val;
	    break;

	case 'A':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_ARRAY);
	    valptr = (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
	    *valptr = val;
	    break;

	case 'S':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_STRUCT);
	    valptr = (xmlrpc_value**) va_arg(*args, xmlrpc_value**);
	    *valptr = val;
	    break;

	case '(':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_ARRAY);
	    parsearray(env, val, format, ')', args);
	    (*format)++;
	    break;

	case '{':
	    XMLRPC_TYPE_CHECK(env, val, XMLRPC_TYPE_STRUCT);
	    parsestruct(env, val, format, '}', args);
	    (*format)++;
	    break;

	default:
	    XMLRPC_FATAL_ERROR("Unknown type code when parsing value");
    }

 cleanup:
}

static void xmlrpc_parse_value_va (xmlrpc_env* env,
				   xmlrpc_value* value,
				   char* format,
				   va_list args)
{
    char *format_copy;
    va_list args_copy;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(value);
    XMLRPC_ASSERT(format != NULL);

    format_copy = format;
    VA_LIST_COPY(args_copy, args);
    parsevalue(env, value, &format_copy, &args_copy);
    XMLRPC_FAIL_IF_FAULT(env);

    XMLRPC_ASSERT(*format_copy == '\0');

 cleanup:
}

void xmlrpc_parse_value (xmlrpc_env* env,
			 xmlrpc_value* value,
			 char* format, ...)
{
    va_list args;

    va_start(args, format);
    xmlrpc_parse_value_va(env, value, format, args);
    va_end(args);
}


/*=========================================================================
**  XML-RPC Array Support
**=========================================================================
*/

int xmlrpc_array_size (xmlrpc_env* env, xmlrpc_value* array)
{
    int retval;

    /* Suppress a compiler warning about uninitialized variables. */
    retval = 0;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(array);
    XMLRPC_TYPE_CHECK(env, array, XMLRPC_TYPE_ARRAY);

    retval = XMLRPC_TYPED_MEM_BLOCK_SIZE(xmlrpc_value*, &array->_block);

 cleanup:
    if (env->fault_occurred)
	return -1;
    else
	return retval;
}

void xmlrpc_array_append_item (xmlrpc_env* env,
			       xmlrpc_value* array,
			       xmlrpc_value* value)
{
    size_t size;
    xmlrpc_value **contents;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(array);
    XMLRPC_TYPE_CHECK(env, array, XMLRPC_TYPE_ARRAY);

    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(xmlrpc_value*, &array->_block);
    XMLRPC_TYPED_MEM_BLOCK_RESIZE(xmlrpc_value*, env, &array->_block, size+1);
    XMLRPC_FAIL_IF_FAULT(env);

    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(xmlrpc_value*, &array->_block);
    xmlrpc_INCREF(value);
    contents[size] = value;

 cleanup:    
}

xmlrpc_value* xmlrpc_array_get_item (xmlrpc_env* env,
				     xmlrpc_value* array,
				     int index)
{
    size_t size;
    xmlrpc_value **contents, *retval;

    /* Suppress a compiler warning about uninitialized variables. */
    retval = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(array);
    XMLRPC_TYPE_CHECK(env, array, XMLRPC_TYPE_ARRAY);

    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(xmlrpc_value*, &array->_block);
    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(xmlrpc_value*, &array->_block);

    if (index >= size)
	XMLRPC_FAIL1(env, XMLRPC_INDEX_ERROR, "Index %d out of bounds", index);
    retval = contents[index];

 cleanup:    
    if (env->fault_occurred)
	return NULL;
    return retval;
}

/*
int xmlrpc_array_set_item (xmlrpc_env* env,
			   xmlrpc_value* array,
			   int index,
			   xmlrpc_value* value)
{

}
*/

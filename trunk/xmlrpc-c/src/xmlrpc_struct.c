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


#include "xmlrpc_config.h"
#include <stddef.h>
#include <stdlib.h>

#define  XMLRPC_WANT_INTERNAL_DECLARATIONS
#include "xmlrpc.h"

#define KEY_ERROR_BUFFER_SZ (32)

/*=========================================================================
**  xmlrpc_struct_new
**=========================================================================
**  Create a new <struct> value. The corresponding destructor code
**  currently lives in xmlrpc_DECREF.
**
**  We store the individual members in an array of _struct_member. This
**  contains a key, a hash code, and a value. We look up keys by doing
**  a linear search of the hash codes.
*/

xmlrpc_value *xmlrpc_struct_new (xmlrpc_env* env)
{
    xmlrpc_value *strct;
    int strct_valid;

    XMLRPC_ASSERT_ENV_OK(env);

    /* Set up error handling preconditions. */
    strct = NULL;
    strct_valid = 0;

    /* Allocate and fill out an empty structure. */
    strct = (xmlrpc_value*) malloc(sizeof(xmlrpc_value));
    XMLRPC_FAIL_IF_NULL(strct, env, XMLRPC_INTERNAL_ERROR,
			"Could not allocate memory for struct");
    strct->_refcount = 1;
    strct->_type = XMLRPC_TYPE_STRUCT;
    XMLRPC_TYPED_MEM_BLOCK_INIT(_struct_member, env, &strct->_block, 0);
    XMLRPC_FAIL_IF_FAULT(env);
    strct_valid = 1;

 cleanup:
    if (env->fault_occurred) {
	if (strct) {
	    if (strct_valid)
		xmlrpc_DECREF(strct);
	    else
		free(strct);
	}
	return NULL;
    }
    return strct;
}


/*=========================================================================
**  xmlrpc_struct_size
**=========================================================================
**  Return the number of key-value pairs contained in the struct. If the
**  value is not a struct, return -1 and set a fault.
*/

int xmlrpc_struct_size (xmlrpc_env* env, xmlrpc_value* strct)
{
    int retval;

    /* Suppress a compiler warning about uninitialized variables. */
    retval = 0;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(strct);

    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);
    retval = XMLRPC_TYPED_MEM_BLOCK_SIZE(_struct_member, &strct->_block);

 cleanup:
    if (env->fault_occurred)
	return -1;
    return retval;
}


/*=========================================================================
**  get_hash
**=========================================================================
**  A mindlessly simple hash function. Please feel free to write something
**  more clever if this produces bad results.
*/

static unsigned char get_hash (char *key, size_t key_len)
{
    unsigned char retval;
    size_t i;

    XMLRPC_ASSERT(key != NULL);
    
    retval = 0;
    for (i = 0; i < key_len; i++)
	retval += key[i];
    return retval;
}


/*=========================================================================
**  find_member
**=========================================================================
**  Get the index of the member with the specified key, or -1 if no such
**  member exists.
*/

static int find_member (xmlrpc_value *strct, char *key, size_t key_len)
{
    size_t size, i;
    unsigned char hash;
    _struct_member *contents;
    xmlrpc_value *keyval;
    char *keystr;
    size_t keystr_size;

    XMLRPC_ASSERT_VALUE_OK(strct);
    XMLRPC_ASSERT(key != NULL && key_len >= 0);

    /* Look for our key. */
    hash = get_hash(key, key_len);
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(_struct_member, &strct->_block);
    contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(_struct_member, &strct->_block);
    for (i = 0; i < size; i++) {
	if (contents[i].key_hash == hash) {
	    keyval = contents[i].key;
	    keystr = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &keyval->_block);
	    keystr_size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &keyval->_block)-1;
	    if (key_len == keystr_size && memcmp(key, keystr, key_len) == 0)
		return i;
	}   
    }

    return -1;
}


/*=========================================================================
**  xmlrpc_struct_has_key
**=========================================================================
*/

int xmlrpc_struct_has_key (xmlrpc_env* env,
			   xmlrpc_value* strct,
			   char* key)
{
    XMLRPC_ASSERT(key != NULL);
    return xmlrpc_struct_has_key_n(env, strct, key, strlen(key));
}

int xmlrpc_struct_has_key_n (xmlrpc_env* env,
			     xmlrpc_value* strct,
			     char* key, size_t key_len)
{
    int index;

    /* Suppress a compiler warning about uninitialized variables. */
    index = 0;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(strct);
    XMLRPC_ASSERT(key != NULL);
    
    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);
    index = find_member(strct, key, key_len);

 cleanup:
    if (env->fault_occurred)
	return 0;
    return (index >= 0);
}


/*=========================================================================
**  xmlrpc_struct_get_value
**=========================================================================
**  Given a key, retrieve a value from the struct. If the key is not
**  present, set a fault and return NULL.
*/

xmlrpc_value* xmlrpc_struct_get_value (xmlrpc_env* env,
				       xmlrpc_value* strct,
				       char* key)
{
    XMLRPC_ASSERT(key != NULL);
    return xmlrpc_struct_get_value_n(env, strct, key, strlen(key));
}

xmlrpc_value* xmlrpc_struct_get_value_n (xmlrpc_env* env,
					 xmlrpc_value* strct,
					 char* key, size_t key_len)
{
    int index;
    _struct_member *members;
    xmlrpc_value *retval;

    /* Suppress a compiler warning about uninitialized variables. */
    retval = NULL;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(strct);
    XMLRPC_ASSERT(key != NULL);

    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);

    /* Get our member index. */
    index = find_member(strct, key, key_len);
    if (index < 0) {
	/* Create a NULL-terminated version of our key, and report the error.
	** XXX - This is dangerous code (because some cases happen so
	** infrequently that they're hard to test), but the debugging help
	** is invaluable. So we really need it; just be very careful if you
	** change it.
	** XXX - We report a misleading error message if the key contains
	** '\0' characters.) */
	char buffer[KEY_ERROR_BUFFER_SZ];
	if (key_len > KEY_ERROR_BUFFER_SZ - 1)
	    key_len = KEY_ERROR_BUFFER_SZ - 1;
	memcpy(buffer, key, key_len);
	buffer[key_len] = 0;
	XMLRPC_FAIL1(env, XMLRPC_INDEX_ERROR,
		     "No struct member %s...", buffer);
    }

    /* Recover our actual value. */
    members = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(_struct_member, &strct->_block);
    retval = members[index].value;

    XMLRPC_ASSERT_VALUE_OK(retval);

 cleanup:
    if (env->fault_occurred)
	return NULL;
    return retval;
}


/*=========================================================================
**  xmlrpc_struct_set_value
**=========================================================================
*/

void xmlrpc_struct_set_value (xmlrpc_env* env,
			      xmlrpc_value* strct,
			      char* key,
			      xmlrpc_value* value)
{
    XMLRPC_ASSERT(key != NULL);
    xmlrpc_struct_set_value_n(env, strct, key, strlen(key), value);    
}

void xmlrpc_struct_set_value_n (xmlrpc_env* env,
				xmlrpc_value* strct,
				char* key, size_t key_len,
				xmlrpc_value* value)
{
    xmlrpc_value *keyval;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT(key != NULL);

    /* Set up error handling preconditions. */
    keyval = NULL;

    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);

    /* Build an xmlrpc_value from our string. */
    keyval = xmlrpc_build_value(env, "s#", key, key_len);
    XMLRPC_FAIL_IF_FAULT(env);

    /* Do the actual work. */
    xmlrpc_struct_set_value_v(env, strct, keyval, value);

 cleanup:
    if (keyval)
	xmlrpc_DECREF(keyval);
}

void xmlrpc_struct_set_value_v (xmlrpc_env* env,
				xmlrpc_value* strct,
				xmlrpc_value* keyval,
				xmlrpc_value* value)
{
    char *key;
    size_t key_len;
    int index;
    _struct_member *members, *member, new_member;
    xmlrpc_value *old_value;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(strct);
    XMLRPC_ASSERT_VALUE_OK(keyval);
    XMLRPC_ASSERT_VALUE_OK(value);

    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);
    XMLRPC_TYPE_CHECK(env, keyval, XMLRPC_TYPE_STRING);

    key = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, &keyval->_block);
    key_len = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, &keyval->_block) - 1;
    index = find_member(strct, key, key_len);

    if (index >= 0) {

	/* Change the value of an existing member. (But be careful--the
	** original and new values might be the same object, so watch
	** the order of INCREF and DECREF calls!) */
	members = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(_struct_member,
						  &strct->_block);
	member = &members[index];

	/* Juggle our references. */
	old_value = member->value;
	member->value = value;
	xmlrpc_INCREF(member->value);
	xmlrpc_DECREF(old_value);

    } else {

	/* Add a new member. */
	new_member.key_hash = get_hash(key, key_len);
	new_member.key      = keyval;
	new_member.value    = value;
	XMLRPC_TYPED_MEM_BLOCK_APPEND(_struct_member, env, &strct->_block,
				      &new_member, 1);
	XMLRPC_FAIL_IF_FAULT(env);
	xmlrpc_INCREF(keyval);
	xmlrpc_INCREF(value);
    }

 cleanup:
}

/*=========================================================================
**  xmlrpc_struct_get_key_and_value
**=========================================================================
**  Given an index, look up the corresponding key and value. This code is
**  used to help serialize structs. The order of keys and values is
**  undefined, and may change after a call to xmlrpc_struct_set_value*.
**
**  If the specified index does not exist, set the key and value to NULL.
**  As usual, we do *not* increment reference counts for the returned
**  values.
*/

void xmlrpc_struct_get_key_and_value (xmlrpc_env *env,
				      xmlrpc_value *strct,
				      int index,
				      xmlrpc_value **keyval,
				      xmlrpc_value **value)
{
    _struct_member *members, *member;
    size_t size;

    XMLRPC_ASSERT_ENV_OK(env);
    XMLRPC_ASSERT_VALUE_OK(strct);
    XMLRPC_ASSERT(keyval != NULL && value != NULL);

    XMLRPC_TYPE_CHECK(env, strct, XMLRPC_TYPE_STRUCT);

    members = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(_struct_member, &strct->_block);
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(_struct_member, &strct->_block);

    if (index < 0 || index >= size)
	XMLRPC_FAIL(env, XMLRPC_INDEX_ERROR, "Invalid index into struct");
    
    member = &members[index];
    *keyval = member->key;
    *value = member->value;

 cleanup:
    if (env->fault_occurred) {
	*keyval = NULL;
	*value = NULL;
    }
}

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef HAVE_WIN32_CONFIG_H
#include "xmlrpc_config.h"
#else
#include "xmlrpc_win32_config.h"
#endif

#include "xmlrpc.h"
#include "xmlrpc_server.h"
#include "xmlrpc_xmlparser.h"

#define CRLF    "\015\012"
#define INT_MAX (2147483647)
#define INT_MIN (-INT_MAX - 1)


/*=========================================================================
**  Test Harness
**=========================================================================
**  This is a super light-weight test harness. It's vaguely inspired by
**  Kent Beck's book on eXtreme Programming (XP)--the output is succinct,
**  new tests can be coded quickly, and the whole thing runs in a few
**  second's time.
**
**  To run the tests, type './rpctest'.
**  To check for memory leaks, install RedHat's 'memprof' utility, and
**  type 'memprof rpctest'.
**
**  If you add new tests to this file, please deallocate any data
**  structures you use in the appropriate fashion. This allows us to test
**  various destructor code for memory leaks.
*/

int total_tests = 0;
int total_failures = 0;

/* This is a good place to set a breakpoint. */
static void test_failure (char *file, int line, char *label, char *statement)
{
    total_failures++;
    printf("\n%s:%d: test failure: %s (%s)\n", file, line, label, statement);
    exit(1);
}

#define TEST(statement) \
do { \
    total_tests++; \
    if ((statement)) { \
        printf("."); \
    } else { \
        test_failure(__FILE__, __LINE__, "expected", #statement); \
    } \
   } while (0)

#define TEST_NO_FAULT(env) \
    do { \
        total_tests++; \
        if (!(env)->fault_occurred) { \
            printf("."); \
        } else { \
            test_failure(__FILE__, __LINE__, "fault occurred", \
            (env)->fault_string); \
        } \
       } while (0)


/*=========================================================================
**  Test Data
**=========================================================================
**  Some common test data which need to be allocated at a fixed address,
**  or which are inconvenient to allocate inline.
*/

    static char* test_string_1 = "foo";
static char* test_string_2 = "bar";
static int test_int_array_1[5] = {1, 2, 3, 4, 5};
static int test_int_array_2[3] = {6, 7, 8};
static int test_int_array_3[8] = {1, 2, 3, 4, 5, 6, 7, 8};

/* We use these strings for simple serialization and deserialization tests. */
#define RAW_STRING_DATA \
    "<value><array><data>"CRLF \
    "<value><i4>2147483647</i4></value>"CRLF \
    "<value><i4>-2147483648</i4></value>"CRLF \
    "<value><boolean>0</boolean></value>"CRLF \
    "<value><boolean>1</boolean></value>"CRLF \
    "<value><string>Hello, world! &lt;&amp;&gt;</string></value>"CRLF \
    "<value><base64>"CRLF \
    "YmFzZTY0IGRhdGE="CRLF \
    "</base64></value>"CRLF \
    "<value><dateTime.iso8601>19980717T14:08:55</dateTime.iso8601></value>"CRLF \
    "<value><array><data>"CRLF \
    "</data></array></value>"CRLF \
    "</data></array></value>"

#define XML_PROLOGUE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"CRLF

static char serialized_call[] =
    XML_PROLOGUE
    "<methodCall>"CRLF
    "<methodName>gloom&amp;doom</methodName>"CRLF
    "<params>"CRLF
    "<param><value><i4>10</i4></value></param>"CRLF
    "<param><value><i4>20</i4></value></param>"CRLF
    "</params>"CRLF
    "</methodCall>"CRLF;

static char serialized_response[] =
    XML_PROLOGUE
    "<methodResponse>"CRLF
    "<params>"CRLF
    "<param><value><i4>30</i4></value></param>"CRLF
    "</params>"CRLF
    "</methodResponse>"CRLF;

static char serialized_fault[] =
    XML_PROLOGUE
    "<methodResponse>"CRLF
    "<fault>"CRLF
    "<value><struct>"CRLF
    "<member><name>faultCode</name>"CRLF
    "<value><i4>6</i4></value></member>"CRLF
    "<member><name>faultString</name>"CRLF
    "<value><string>A fault occurred</string></value></member>"CRLF
    "</struct></value>"CRLF
    "</fault>"CRLF
    "</methodResponse>"CRLF;

static char expat_data[] = XML_PROLOGUE RAW_STRING_DATA CRLF;
static char expat_error_data[] =
    XML_PROLOGUE \
    "<foo><bar>abc</bar><baz></baz>"CRLF;

static char correct_value[] = 
    XML_PROLOGUE 
    "<methodResponse><params><param>"CRLF 
    "<value><array><data>"CRLF 
    RAW_STRING_DATA CRLF 
    "<value><int>1</int></value>"CRLF 
    "<value><double>-1.0</double></value>"CRLF 
    "<value><double>0.0</double></value>"CRLF 
    "<value><double>1.0</double></value>"CRLF 
    "<value><struct>"CRLF 
    "<member><name>ten &lt;&amp;&gt;</name>"CRLF 
    "<value><i4>10</i4></value></member>"CRLF 
    "<member><name>twenty</name>"CRLF 
    "<value><i4>20</i4></value></member>"CRLF 
    "</struct></value>"CRLF 
    "<value>Untagged string</value>"CRLF 
    "</data></array></value>"CRLF 
    "</param></params></methodResponse>"CRLF;

#define VALUE_HEADER \
    XML_PROLOGUE"<methodResponse><params><param><value>"CRLF
#define VALUE_FOOTER \
    "</value></param></params></methodResponse>"CRLF

#define MEMBER_HEADER \
    VALUE_HEADER"<struct><member>"
#define MEMBER_FOOTER \
    "</member></struct>"VALUE_FOOTER
#define ARBITRARY_VALUE \
    "<value><i4>0</i4></value>"

static char unparseable_value[] = VALUE_HEADER"<i4>"VALUE_FOOTER;

static char *(bad_values[]) = {
    VALUE_HEADER"<i4>0</i4><i4>0</i4>"VALUE_FOOTER,
    VALUE_HEADER"<foo></foo>"VALUE_FOOTER,
    VALUE_HEADER"<i4><i4>4</i4></i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>2147483648</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>-2147483649</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4> 0</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>0 </i4>"VALUE_FOOTER,
    VALUE_HEADER"<boolean>2</boolean>"VALUE_FOOTER,
    VALUE_HEADER"<boolean>-1</boolean>"VALUE_FOOTER,
    VALUE_HEADER"<double> 0.0</double>"VALUE_FOOTER,
    VALUE_HEADER"<double>0.0 </double>"VALUE_FOOTER,
    VALUE_HEADER"<array></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data></data><data></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data></data><data></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data><foo></foo></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<struct><foo></foo></struct>"VALUE_FOOTER,
    MEMBER_HEADER MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name>"MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name>"ARBITRARY_VALUE"<f></f>"MEMBER_FOOTER,
    MEMBER_HEADER"<foo></foo>"ARBITRARY_VALUE MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name><foo></foo>"MEMBER_FOOTER,
    MEMBER_HEADER"<name><foo></foo></name>"ARBITRARY_VALUE MEMBER_FOOTER,
    NULL
};

#define RESPONSE_HEADER \
    XML_PROLOGUE"<methodResponse>"CRLF
#define RESPONSE_FOOTER \
    "</methodResponse>"CRLF

#define PARAMS_RESP_HEADER \
    RESPONSE_HEADER"<params>"
#define PARAMS_RESP_FOOTER \
    "</params>"RESPONSE_FOOTER

#define FAULT_HEADER \
    RESPONSE_HEADER"<fault>"
#define FAULT_FOOTER \
    "</fault>"RESPONSE_FOOTER

#define FAULT_STRUCT_HEADER \
    FAULT_HEADER"<value><struct>"
#define FAULT_STRUCT_FOOTER \
    "</struct></value>"FAULT_FOOTER

static char *(bad_responses[]) = {
    XML_PROLOGUE"<foo></foo>"CRLF,
    RESPONSE_HEADER RESPONSE_FOOTER,
    RESPONSE_HEADER"<params></params><params></params>"RESPONSE_FOOTER,
    RESPONSE_HEADER"<foo></foo>"RESPONSE_FOOTER,

    /* Make sure we insist on only one parameter in a response. */
    PARAMS_RESP_HEADER PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER
    "<param><i4>0</i4></param>"
    "<param><i4>0</i4></param>"
    PARAMS_RESP_FOOTER,

    /* Test other sorts of bad parameters. */
    PARAMS_RESP_HEADER"<foo></foo>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER"<param></param>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER"<param><foo></foo></param>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER
    "<param>"ARBITRARY_VALUE ARBITRARY_VALUE"</param>"
    PARAMS_RESP_FOOTER,
    
    /* Basic fault tests. */
    FAULT_HEADER FAULT_FOOTER,
    FAULT_HEADER"<foo></foo>"FAULT_FOOTER,
    FAULT_HEADER"<value></value><value></value>"FAULT_FOOTER,
    FAULT_HEADER"<value><i4>1</i4></value>"FAULT_FOOTER,

    /* Make sure we insist on the proper members within the fault struct. */
    FAULT_STRUCT_HEADER
    "<member><name>faultString</name>"
    "<value><string>foo</string></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><i4>0</i4></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><i4>0</i4></value></member>"
    "<member><name>faultString</name>"
    "<value><i4>0</i4></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><string>0</string></value></member>"
    "<member><name>faultString</name>"
    "<value><string>foo</string></value></member>"
    FAULT_STRUCT_FOOTER,
    NULL};

#define CALL_HEADER \
    XML_PROLOGUE"<methodCall>"CRLF
#define CALL_FOOTER \
    "</methodCall>"CRLF

static char *(bad_calls[]) = {
    XML_PROLOGUE"<foo></foo>"CRLF,
    CALL_HEADER CALL_FOOTER,
    CALL_HEADER"<methodName>m</methodName><foo></foo>"CALL_FOOTER, 
    CALL_HEADER"<foo></foo><params></params>"CALL_FOOTER, 
    CALL_HEADER"<methodName><f></f></methodName><params></params>"CALL_FOOTER, 
    NULL};


/*=========================================================================
**  Test Suites
**=========================================================================
*/

static void test_env(void)
{
    xmlrpc_env env, env2;
    char *s;

    /* Test xmlrpc_env_init. */
    xmlrpc_env_init(&env);
    TEST(!env.fault_occurred);
    TEST(env.fault_code == 0);
    TEST(env.fault_string == NULL);

    /* Test xmlrpc_set_fault. */
    xmlrpc_env_set_fault(&env, 1, test_string_1);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 1);
    TEST(env.fault_string != test_string_1);
    TEST(strcmp(env.fault_string, test_string_1) == 0);

    /* Change an existing fault. */
    xmlrpc_env_set_fault(&env, 2, test_string_2);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 2);
    TEST(strcmp(env.fault_string, test_string_2) == 0);    

    /* Set a fault with a format string. */
    xmlrpc_env_set_fault_formatted(&env, 3, "a%s%d", "bar", 9);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 3);
    TEST(strcmp(env.fault_string, "abar9") == 0);

    /* Set a fault with an oversized string. */
    s = "12345678901234567890123456789012345678901234567890";
    xmlrpc_env_set_fault_formatted(&env, 4, "%s%s%s%s%s%s", s, s, s, s, s, s);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 4);
    TEST(strlen(env.fault_string) == 255);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_env_clean(&env);

    /* Test cleanup code on in absence of xmlrpc_env_set_fault. */
    xmlrpc_env_init(&env2);
    xmlrpc_env_clean(&env2);
}

static void test_mem_block (void)
{
    xmlrpc_env env;
    xmlrpc_mem_block* block;

    xmlrpc_mem_block* typed_heap_block;
    xmlrpc_mem_block typed_auto_block;
    void** typed_contents;

    xmlrpc_env_init(&env);

    /* Allocate a zero-size block. */
    block = xmlrpc_mem_block_new(&env, 0);
    TEST_NO_FAULT(&env);
    TEST(block != NULL);
    TEST(xmlrpc_mem_block_size(block) == 0);

    /* Grow the block a little bit. */
    xmlrpc_mem_block_resize(&env, block, strlen(test_string_1) + 1);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(block) == strlen(test_string_1) + 1);
    
    /* Insert a string into the block, and resize it by large amount.
    ** We want to cause a reallocation and copy of the block contents. */
    strcpy(xmlrpc_mem_block_contents(block), test_string_1);
    xmlrpc_mem_block_resize(&env, block, 10000);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(block) == 10000);
    TEST(strcmp(xmlrpc_mem_block_contents(block), test_string_1) == 0);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(block);
    
    /* Allocate a bigger block. */
    block = xmlrpc_mem_block_new(&env, 128);
    TEST_NO_FAULT(&env);
    TEST(block != NULL);
    TEST(xmlrpc_mem_block_size(block) == 128);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(block);

    /* Allocate a "typed" memory block. */
    typed_heap_block = XMLRPC_TYPED_MEM_BLOCK_NEW(void*, &env, 20);
    TEST_NO_FAULT(&env);
    TEST(typed_heap_block != NULL);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typed_heap_block) == 20);
    typed_contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(void*, typed_heap_block);
    TEST(typed_contents != NULL);

    /* Resize a typed memory block. */
    XMLRPC_TYPED_MEM_BLOCK_RESIZE(void*, &env, typed_heap_block, 100);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typed_heap_block) == 100);

    /* Test cleanup code (with help from memprof). */
    XMLRPC_TYPED_MEM_BLOCK_FREE(void*, typed_heap_block);

    /* Test _INIT and _CLEAN for stack-based memory blocks. */
    XMLRPC_TYPED_MEM_BLOCK_INIT(void*, &env, &typed_auto_block, 30);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, &typed_auto_block) == 30);
    XMLRPC_TYPED_MEM_BLOCK_CLEAN(void*, &typed_auto_block);

    /* Test xmlrpc_mem_block_append. */
    block = XMLRPC_TYPED_MEM_BLOCK_NEW(int, &env, 5);
    TEST_NO_FAULT(&env);
    memcpy(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, block),
           test_int_array_1, sizeof(test_int_array_1));
    XMLRPC_TYPED_MEM_BLOCK_APPEND(int, &env, block, test_int_array_2, 3);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(int, block) == 8);
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, block),
                test_int_array_3, sizeof(test_int_array_3)) == 0);
    XMLRPC_TYPED_MEM_BLOCK_FREE(int, block);

    xmlrpc_env_clean(&env);
}

static char *(base64_triplets[]) = {
    "", "", CRLF,
    "a", "YQ==", "YQ=="CRLF,
    "aa", "YWE=", "YWE="CRLF,
    "aaa", "YWFh", "YWFh"CRLF,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWmFiY"
    "2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVo=",
    "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWmFiY"
    "2Rl"CRLF
    "ZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVo="CRLF,
    NULL};

static void test_base64_conversion (void)
{
    xmlrpc_env env, env2;
    char **triplet, *bin_data, *nocrlf_ascii_data, *ascii_data;
    xmlrpc_mem_block *output;

    xmlrpc_env_init(&env);

    for (triplet = base64_triplets; *triplet != NULL; triplet += 3) {
        bin_data = *triplet;
        nocrlf_ascii_data = *(triplet + 1);
        ascii_data = *(triplet + 2);

        /* Test our encoding routine. */
        output = xmlrpc_base64_encode(&env,
                                      (unsigned char*) bin_data,
                                      strlen(bin_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(ascii_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), ascii_data,
                    strlen(ascii_data)) == 0);
        xmlrpc_mem_block_free(output);

        /* Test our newline-free encoding routine. */
        output =
            xmlrpc_base64_encode_without_newlines(&env,
                                                  (unsigned char*) bin_data,
                                                  strlen(bin_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(nocrlf_ascii_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), nocrlf_ascii_data,
                    strlen(nocrlf_ascii_data)) == 0);
        xmlrpc_mem_block_free(output);

        /* Test our decoding routine. */
        output = xmlrpc_base64_decode(&env, ascii_data, strlen(ascii_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(bin_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), bin_data,
                    strlen(bin_data)) == 0);
        xmlrpc_mem_block_free(output);
    }

    /* Now for something broken... */
    xmlrpc_env_init(&env2);
    output = xmlrpc_base64_decode(&env2, "====", 4);
    TEST(output == NULL);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Now for something broken in a really sneaky way... */
    xmlrpc_env_init(&env2);
    output = xmlrpc_base64_decode(&env2, "a==", 4);
    TEST(output == NULL);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_clean(&env);
}


static void
test_value_alloc_dealloc(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    /* Test allocation and deallocation (w/memprof). */
    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    xmlrpc_INCREF(v);
    xmlrpc_DECREF(v);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}


static void
test_value_integer(void) { 

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_int32 i;

    xmlrpc_env_init(&env);

    /* Test integers. */
    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 10);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_INT == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "i", &i);
    TEST_NO_FAULT(&env);
    TEST(i == 10);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_bool(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_bool b;

    /* Test booleans. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "b", (xmlrpc_bool) 1);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_BOOL == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "b", &b);
    TEST_NO_FAULT(&env);
    TEST(b);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_double(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    double d;

    /* Test doubles. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "d", 1.0);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_DOUBLE == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "d", &d);
    TEST_NO_FAULT(&env);
    TEST(d == 1.0);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_no_null(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    char * str;
    size_t len;

    /* Test strings (without '\0' bytes). */
    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "s", test_string_1);

    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "s", &str);
    TEST_NO_FAULT(&env);
    TEST(strcmp(str, test_string_1) == 0);
    xmlrpc_parse_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(memcmp(str, test_string_1, strlen(test_string_1)) == 0);
    TEST(strlen(str) == strlen(test_string_1));
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_null(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_env env2;
    char * str;
    size_t len;

    /* Test a string with a '\0' byte. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "s#", "foo\0bar", (size_t) 7);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(memcmp(str, "foo\0bar", 7) == 0);
    TEST(len == 7);

    /* Test for type error when decoding a string with a zero byte to a
    ** regular C string. */
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, v, "s", &str);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_value(void) {

    xmlrpc_value *v, *v2, *v3;
    xmlrpc_env env;

    /* Test 'V' with building and parsing. */

    xmlrpc_env_init(&env);

    v2 = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "V", v2);
    TEST_NO_FAULT(&env);
    TEST(v == v2);
    xmlrpc_parse_value(&env, v2, "V", &v3);
    TEST_NO_FAULT(&env);
    TEST(v2 == v3);
    xmlrpc_DECREF(v);
    xmlrpc_DECREF(v2);

    xmlrpc_env_clean(&env);
}



static void
test_value_array(void) {

    xmlrpc_value *v;
    xmlrpc_env env;
    size_t len;

    /* Basic array-building test. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "()");
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v));
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_AS(void) {

    xmlrpc_value *v;
    xmlrpc_value *v2;
    xmlrpc_value *v3;
    xmlrpc_env env;
    size_t len;

    /* Test parsing of 'A' and 'S'. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "((){})");
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, v, "(AS)", &v2, &v3);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v2));
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(v3));
    len = xmlrpc_array_size(&env, v2);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    len = xmlrpc_struct_size(&env, v3);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_AS_typecheck(void) {

    xmlrpc_env env;
    xmlrpc_env env2;
    xmlrpc_value *v;
    xmlrpc_value *v2;

    /* Test typechecks for 'A' and 'S'. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "s", "foo");
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, v, "A", &v2);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, v, "S", &v2);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);
    xmlrpc_DECREF(v);
    xmlrpc_env_clean(&env);
}



static void
test_value_array2(void) {

    xmlrpc_value *v;
    xmlrpc_env env;
    xmlrpc_int32 i, i1, i2, i3, i4;
    xmlrpc_value * item;
    size_t len;

    /* A more complex array. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "(i(ii)i)",
                           (xmlrpc_int32) 10, (xmlrpc_int32) 20,
                           (xmlrpc_int32) 30, (xmlrpc_int32) 40);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v));
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 3);
    item = xmlrpc_array_get_item(&env, v, 1);
    TEST_NO_FAULT(&env);
    len = xmlrpc_array_size(&env, item);
    TEST_NO_FAULT(&env);
    TEST(len == 2);
    item = xmlrpc_array_get_item(&env, item, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, item, "i", &i);
    TEST_NO_FAULT(&env);
    TEST(i == 20);
    xmlrpc_parse_value(&env, v, "(i(ii)i)", &i1, &i2, &i3, &i4);
    TEST_NO_FAULT(&env);
    TEST(i1 == 10 && i2 == 20 && i3 == 30 && i4 == 40);
    xmlrpc_parse_value(&env, v, "(i(i*)i)", &i1, &i2, &i3);
    TEST_NO_FAULT(&env);
    TEST(i1 == 10 && i2 == 20 && i3 == 40);
    xmlrpc_parse_value(&env, v, "(i(ii*)i)", &i1, &i2, &i3, &i4);
    TEST_NO_FAULT(&env);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_type_mismatch(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_env env2;
    char * str;

    /* Test for one, simple kind of type mismatch error. We assume that
    ** if one of these typechecks works, the rest work fine. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, v, "s", &str);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_DECREF(v);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_clean(&env);
}



static void
test_value_cptr(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    void * ptr;

    /* Test C pointer storage using 'p'.
    ** We don't support cleanup functions (yet). */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "p", (void*) 0x00000017);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_C_PTR == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "p", &ptr);
    TEST_NO_FAULT(&env);
    TEST(ptr == (void*) 0x00000017);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_base64(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    unsigned char* data;
    size_t len;

    /* Test <base64> data. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "6", "a\0b", (size_t) 3);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_BASE64 == xmlrpc_value_type(v));
    xmlrpc_parse_value(&env, v, "6", &data, &len);
    TEST_NO_FAULT(&env);
    TEST(len == 3);
    TEST(memcmp(data, "a\0b", len) == 0);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_invalid_type(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    /* Test invalid type specifier in format string */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "Q");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_value_missing_array_delim(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    /* Test missing close parenthesis on array */

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "(");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "(i");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);
}



static void
test_value_missing_struct_delim(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    
    /* Test missing closing brace on struct */

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:i", "key1", 7);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:i,s:i", "key1", 9, "key2", -4);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);
}



static void
test_value_invalid_struct(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    
    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:ii");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{si:");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s");
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);
}



static void test_value (void)
{
    printf("\nRunning value tests.");

    test_value_alloc_dealloc();
    test_value_integer();
    test_value_bool();
    test_value_double();
    test_value_string_no_null();
    test_value_string_null();
    test_value_type_mismatch();
    test_value_value();
    test_value_array();
    test_value_array2();
    test_value_AS();
    test_value_AS_typecheck();
    test_value_cptr();
    test_value_base64();
    test_value_invalid_type();
    test_value_missing_array_delim();
    test_value_missing_struct_delim();
    test_value_invalid_struct();
    
    printf("\nValue tests done.");
}



static void test_bounds_checks (void)
{
    xmlrpc_env env;
    xmlrpc_value *array;
    int i1, i2, i3, i4;

    /* Get an array to work with. */
    xmlrpc_env_init(&env);
    array = xmlrpc_build_value(&env, "(iii)", 100, 200, 300);
    TEST_NO_FAULT(&env);
    xmlrpc_env_clean(&env);
    
    /* Test bounds check on xmlrpc_array_get_item. */
    xmlrpc_env_init(&env);
    xmlrpc_array_get_item(&env, array, 3);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);

    /* Test xmlrpc_parse_value with too few values. */
    xmlrpc_env_init(&env);
    xmlrpc_parse_value(&env, array, "(iiii)", &i1, &i2, &i3, &i4);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);

    /* Test xmlrpc_parse_value with too many values. */
    xmlrpc_env_init(&env);
    xmlrpc_parse_value(&env, array, "(ii)", &i1, &i2, &i3, &i4);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);

    /* Dispose of our array. */
    xmlrpc_DECREF(array);
}

static void test_struct (void)
{
    xmlrpc_env env, env2;
    xmlrpc_value *s, *i, *i1, *i2, *i3, *key, *value;
    size_t size;
    int present;
    xmlrpc_int32 ival;
    xmlrpc_bool bval;
    char *sval;
    int index;

    xmlrpc_env_init(&env);

    /* Create a struct. */
    s = xmlrpc_struct_new(&env);
    TEST_NO_FAULT(&env);
    TEST(s != NULL);
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(s));
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 0);

    /* Create some elements to insert into our struct. */
    i1 = xmlrpc_build_value(&env, "s", "Item #1");
    TEST_NO_FAULT(&env);
    i2 = xmlrpc_build_value(&env, "s", "Item #2");
    TEST_NO_FAULT(&env);
    i3 = xmlrpc_build_value(&env, "s", "Item #3");
    TEST_NO_FAULT(&env);

    /* Insert a single item. */
    xmlrpc_struct_set_value(&env, s, "foo", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 1);

    /* Insert two more items with conflicting hash codes. (We assume that
    ** nobody has changed the hash function.) */
    xmlrpc_struct_set_value(&env, s, "bar", i2);
    TEST_NO_FAULT(&env);
    xmlrpc_struct_set_value(&env, s, "aas", i3);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);

    /* Replace an existing element with a different element. */
    xmlrpc_struct_set_value(&env, s, "aas", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);

    /* Get an element. */
    i = xmlrpc_struct_get_value(&env, s, "aas");
    TEST_NO_FAULT(&env);
    TEST(i == i1);

    /* Replace an existing element with the same element (tricky). */
    xmlrpc_struct_set_value(&env, s, "aas", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);
    i = xmlrpc_struct_get_value(&env, s, "aas");
    TEST_NO_FAULT(&env);
    TEST(i == i1);

    /* Test for the presence and absence of elements. */
    present = xmlrpc_struct_has_key(&env, s, "aas");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "bogus");
    TEST_NO_FAULT(&env);
    TEST(!present);

    /* Make sure our typechecks work correctly. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_size(&env2, i1);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_has_key(&env2, i1, "foo");
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    i = xmlrpc_struct_get_value(&env2, i1, "foo");
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_set_value(&env2, i1, "foo", i2);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_set_value_v(&env2, s, s, i2);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Attempt to access a non-existant element. */
    xmlrpc_env_init(&env2);
    i = xmlrpc_struct_get_value(&env2, s, "bogus");
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env2);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(s);

    /* Build a struct using our automagic struct builder. */
    s = xmlrpc_build_value(&env, "{s:s,s:i,s:b}",
                           "foo", "Hello!",
                           "bar", (xmlrpc_int32) 1,
                           "baz", (xmlrpc_bool) 0);
    TEST_NO_FAULT(&env);
    TEST(s != NULL);
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(s));
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);
    present = xmlrpc_struct_has_key(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "bar");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "baz");
    TEST_NO_FAULT(&env);
    TEST(present);
    i = xmlrpc_struct_get_value(&env, s, "baz");
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, i, "b", &bval);
    TEST_NO_FAULT(&env);
    TEST(!bval);

    /* Extract keys and values. */
    for (index = 0; index < 3; index++) {
        xmlrpc_struct_get_key_and_value(&env, s, index, &key, &value);
        TEST_NO_FAULT(&env);
        TEST(key != NULL);
        TEST(value != NULL);
    }

    /* Test our automagic struct parser. */
    xmlrpc_parse_value(&env, s, "{s:b,s:s,s:i,*}",
                       "baz", &bval,
                       "foo", &sval,
                       "bar", &ival);
    TEST_NO_FAULT(&env);
    TEST(ival == 1);
    TEST(!bval);
    TEST(strcmp(sval, "Hello!") == 0);

    /* Test automagic struct parser with value of wrong type. */
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, s, "{s:b,s:i,*}",
                       "baz", &bval,
                       "foo", &sval);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test automagic struct parser with bad key. */
    xmlrpc_env_init(&env2);
    xmlrpc_parse_value(&env2, s, "{s:b,s:i,*}",
                       "baz", &bval,
                       "nosuch", &sval);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test type check. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, i1, 0, &key, &value);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_TYPE_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);
    
    /* Test bounds checks. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, s, -1, &key, &value);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, s, 3, &key, &value);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(s);

    xmlrpc_DECREF(i1);
    xmlrpc_DECREF(i2);
    xmlrpc_DECREF(i3);
    xmlrpc_env_clean(&env);
}




static void
test_serialize_basic(void) {

    char const serialized_data[] = RAW_STRING_DATA;

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    /* Build a nice, messy value to serialize. We should attempt to use
    ** use every data type except double (which doesn't serialize in a
    ** portable manner. */
    v = xmlrpc_build_value(&env, "(iibbs68())",
                           (xmlrpc_int32) INT_MAX, (xmlrpc_int32) INT_MIN,
                           (xmlrpc_bool) 0, (xmlrpc_bool) 1,
                           "Hello, world! <&>",
                           "base64 data", (size_t) 11,
                           "19980717T14:08:55");
    TEST_NO_FAULT(&env);
    
    /* Serialize the value. */
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_data));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_data, size) == 0);
    
    /* (Debugging code to display the value.) */
    /* XMLRPC_TYPED_MEM_BLOCK_APPEND(char, &env, output, "\0", 1);
    ** TEST_NO_FAULT(&env);
    ** printf("%s\n", XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output)); */

    /* Clean up our value. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_double(void) {

    /* Test serialize of a double.  */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    char * result;
        /* serialized result, as asciiz string */
    size_t resultLength;
        /* Length in characters of the serialized result */
    float serializedValue;
    char nextChar;
    int itemsMatched;
    
    xmlrpc_env_init(&env);

    /* Build a double to serialize */
    v = xmlrpc_build_value(&env, "d", 3.14159);
    TEST_NO_FAULT(&env);
    
    /* Serialize the value. */
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value.  Note that because
       doubles aren't precise, this might serialize as 3.1415899999
       or something like that.  So we check it arithmetically.
    */
    resultLength = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    result = malloc(resultLength + 1);

    memcpy(result, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output), 
           resultLength);
    result[resultLength] = '\0';
    
    itemsMatched = sscanf(result, 
                          "<value><double>%f</double></value>" CRLF "%c",
                          &serializedValue, &nextChar);

    TEST(itemsMatched == 1);
    TEST(serializedValue - 3.14159 < .000001);
    /* We'd like to test more precision, but sscanf doesn't do doubles */

    free(result);
    
    /* Clean up our value. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_struct(void) {

    /* Serialize a simple struct. */

    char const serialized_struct[] = 
        "<value><struct>"CRLF \
        "<member><name>&lt;&amp;&gt;</name>"CRLF \
        "<value><i4>10</i4></value></member>"CRLF \
        "</struct></value>";
    
    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "{s:i}", "<&>", (xmlrpc_int32) 10);
    TEST_NO_FAULT(&env);
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_struct));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_struct, size) == 0);
    
    /* Clean up our struct. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_methodResponse(void) {

    /* Serialize a methodResponse. */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 30);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_response(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_response));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_response, size) == 0);

    /* Clean up our methodResponse. */
    xmlrpc_DECREF(v);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_methodCall(void) {

    /* Serialize a methodCall. */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "(ii)", (xmlrpc_int32) 10, (xmlrpc_int32) 20);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_call(&env, output, "gloom&doom", v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_call));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_call, size) == 0);

    /* Clean up our methodCall. */
    xmlrpc_DECREF(v);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_fault(void) {
    /* Serialize a fault. */

    xmlrpc_env env;
    xmlrpc_env fault;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&fault);
    xmlrpc_env_set_fault(&fault, 6, "A fault occurred");
    xmlrpc_serialize_fault(&env, output, &fault);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_fault));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_fault, size) == 0);

    /* Clean up our fault. */
    xmlrpc_env_clean(&fault);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void 
test_serialize(void) {

    printf("\nRunning serialize tests.");

    test_serialize_basic();
    test_serialize_double();
    test_serialize_struct();
    test_serialize_methodResponse();
    test_serialize_methodCall();
    test_serialize_fault();

    printf("\nSerialize tests done.");
}



static void test_expat (void)
{
    xmlrpc_env env;
    xml_element *elem, *array, *data, *value1, *i4;
    char *cdata;
    size_t size;

    xmlrpc_env_init(&env);

    /* Parse a moderately complex XML document. */
    elem = xml_parse(&env, expat_data, strlen(expat_data));
    TEST_NO_FAULT(&env);
    TEST(elem != NULL);

    /* Verify our results. */
    TEST(strcmp(xml_element_name(elem), "value") == 0);
    TEST(xml_element_children_size(elem) == 1);
    array = xml_element_children(elem)[0];
    TEST(strcmp(xml_element_name(array), "array") == 0);
    TEST(xml_element_children_size(array) == 1);
    data = xml_element_children(array)[0];
    TEST(strcmp(xml_element_name(data), "data") == 0);
    TEST(xml_element_children_size(data) > 1);
    value1 = xml_element_children(data)[0];
    TEST(strcmp(xml_element_name(value1), "value") == 0);
    TEST(xml_element_children_size(value1) == 1);
    i4 = xml_element_children(value1)[0];
    TEST(strcmp(xml_element_name(i4), "i4") == 0);
    TEST(xml_element_children_size(i4) == 0);
    cdata = xml_element_cdata(i4);
    size = xml_element_cdata_size(i4);
    TEST(size == strlen("2147483647"));
    TEST(memcmp(cdata, "2147483647", strlen("2147483647")) == 0);

    /* Test cleanup code (w/memprof). */
    xml_element_free(elem);

    /* Try to parse broken XML. We want to know that a proper error occurs,
    ** AND that we don't leak any memory (w/memprof). */
    elem = xml_parse(&env, expat_error_data, strlen(expat_error_data));
    TEST(env.fault_occurred);
    TEST(elem == NULL);

    xmlrpc_env_clean(&env);
}

static void test_parse_xml_value (void)
{
    xmlrpc_env env, env2;
    xmlrpc_value *val, *s, *sval;
    xmlrpc_int32 int_max, int_min, int_one;
    xmlrpc_bool bool_false, bool_true;
    char *str_hello, *str_untagged, *datetime;
    unsigned char *b64_data;
    size_t b64_len;
    double negone, zero, one;
    int size, sval_int;
    char **bad_value;
    xml_element *elem;
    
    xmlrpc_env_init(&env);
    
    /* Parse a correctly-formed response. */
    val = xmlrpc_parse_response(&env, correct_value,
                                strlen(correct_value));
    TEST_NO_FAULT(&env);
    TEST(val != NULL);
    
    /* Analyze it and make sure it contains the correct values. */
    xmlrpc_parse_value(&env, val, "((iibbs68())idddSs)", &int_max, &int_min,
                       &bool_false, &bool_true, &str_hello,
                       &b64_data, &b64_len, &datetime,
                       &int_one, &negone, &zero, &one, &s, &str_untagged);
    TEST_NO_FAULT(&env);
    TEST(int_max == INT_MAX);
    TEST(int_min == INT_MIN);
    TEST(!bool_false);
    TEST(bool_true);
    TEST(strlen(str_hello) == strlen("Hello, world! <&>"));
    TEST(strcmp(str_hello, "Hello, world! <&>") == 0);
    TEST(b64_len == 11);
    TEST(memcmp(b64_data, "base64 data", b64_len) == 0);
    TEST(strcmp(datetime, "19980717T14:08:55") == 0);
    TEST(int_one == 1);
    TEST(negone == -1.0);
    TEST(zero == 0.0);
    TEST(one == 1.0);
    TEST(strcmp(str_untagged, "Untagged string") == 0);
    
    /* Analyze the contents of our struct. */
    TEST(s != NULL);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 2);
    sval = xmlrpc_struct_get_value(&env, s, "ten <&>");
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, sval, "i", &sval_int);
    TEST_NO_FAULT(&env);
    TEST(sval_int == 10);
    sval = xmlrpc_struct_get_value(&env, s, "twenty");
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, sval, "i", &sval_int);
    TEST_NO_FAULT(&env);
    TEST(sval_int == 20);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(val);
    
    /* Test our error-checking code. This is exposed to potentially-malicious
    ** network data, so we need to handle evil data gracefully, without
    ** barfing or leaking memory. (w/memprof) */
    
    /* First, test some poorly-formed XML data. */
    xmlrpc_env_init(&env2);
    val = xmlrpc_parse_response(&env2, unparseable_value,
                                strlen(unparseable_value));
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
    TEST(val == NULL);
    xmlrpc_env_clean(&env2);
    
    /* Next, check for bogus values. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_value = bad_values; *bad_value != NULL; bad_value++) {
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_value, strlen(*bad_value));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        val = xmlrpc_parse_response(&env2, *bad_value, strlen(*bad_value));
        TEST(env2.fault_occurred);
        TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
        TEST(val == NULL);
        xmlrpc_env_clean(&env2);
    }
    
    xmlrpc_env_clean(&env);
}

static void test_parse_xml_response (void)
{
    xmlrpc_env env, env2, fault;
    xmlrpc_value *v;
    int i1;
    char **bad_resp;
    xml_element *elem;

    xmlrpc_env_init(&env);

    /* Parse a valid response. */
    v = xmlrpc_parse_response(&env, serialized_response,
                              strlen(serialized_response));
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    xmlrpc_parse_value(&env, v, "i", &i1);
    TEST_NO_FAULT(&env);
    TEST(i1 == 30);
    xmlrpc_DECREF(v);

    /* Parse a valid fault. */
    xmlrpc_env_init(&fault);
    v = xmlrpc_parse_response(&fault, serialized_fault,
                              strlen(serialized_fault));
    TEST(fault.fault_occurred);
    TEST(fault.fault_code == 6);
    TEST(strcmp(fault.fault_string, "A fault occurred") == 0);
    xmlrpc_env_clean(&fault);

    /* We don't need to test our handling of poorly formatted XML here,
    ** because we already did that in test_parse_xml_value. */

    /* Next, check for bogus responses. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_resp = bad_responses; *bad_resp != NULL; bad_resp++) {
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_resp, strlen(*bad_resp));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        v = xmlrpc_parse_response(&env2, *bad_resp, strlen(*bad_resp));
        TEST(env2.fault_occurred);
        TEST(env2.fault_code != 0); /* We use 0 as a code in our bad faults. */
        TEST(v == NULL);
        xmlrpc_env_clean(&env2);
    }
    
    xmlrpc_env_clean(&env);
}

static void test_parse_xml_call (void)
{
    xmlrpc_env env, env2;
    char *method_name;
    xmlrpc_value *params;
    int i1, i2;
    char **bad_call;
    xml_element *elem;

    xmlrpc_env_init(&env);

    /* Parse a valid call. */
    xmlrpc_parse_call(&env, serialized_call, strlen(serialized_call),
                      &method_name, &params);
    TEST_NO_FAULT(&env);
    TEST(params != NULL);
    xmlrpc_parse_value(&env, params, "(ii)", &i1, &i2);
    TEST_NO_FAULT(&env);
    TEST(strcmp(method_name, "gloom&doom") == 0);
    TEST(i1 == 10 && i2 == 20);
    free(method_name);
    xmlrpc_DECREF(params);    

    /* Test some poorly-formed XML data. */
    xmlrpc_env_init(&env2);
    xmlrpc_parse_call(&env2, unparseable_value, strlen(unparseable_value),
                      &method_name, &params);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
    TEST(method_name == NULL && params == NULL);
    xmlrpc_env_clean(&env2);

    /* Next, check for bogus values. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_call = bad_calls; *bad_call != NULL; bad_call++) {
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_call, strlen(*bad_call));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);

        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        xmlrpc_parse_call(&env2, *bad_call, strlen(*bad_call),
                          &method_name, &params);
        TEST(env2.fault_occurred);
        TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
        TEST(method_name == NULL && params == NULL);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_env_clean(&env);    
}

/*=========================================================================
**  test_method_registry
**=========================================================================
**  We need to define some static callbacks to test this code.
*/

#define FOO_USER_DATA ((void*) 0xF00)
#define BAR_USER_DATA ((void*) 0xBAF)

static xmlrpc_value *test_foo (xmlrpc_env *env,
                               xmlrpc_value *param_array,
                               void *user_data)
{
    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == FOO_USER_DATA);

    xmlrpc_parse_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(env, "i", (xmlrpc_int32) x + y);
}

static xmlrpc_value *test_bar (xmlrpc_env *env,
                               xmlrpc_value *param_array,
                               void *user_data)
{
    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == BAR_USER_DATA);

    xmlrpc_parse_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    xmlrpc_env_set_fault(env, 123, "Test fault");
    return NULL;
}

static xmlrpc_value *
test_default(xmlrpc_env *   const env,
             char *         const host ATTR_UNUSED,
             char *         const method_name ATTR_UNUSED,
             xmlrpc_value * const param_array,
             void *         const user_data) {

    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == FOO_USER_DATA);

    xmlrpc_parse_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(env, "i", 2 * (x + y));
}

static xmlrpc_value *
process_call_helper (xmlrpc_env *env,
                     xmlrpc_registry *registry,
                     char *method_name,
                     xmlrpc_value *arg_array)
{
    xmlrpc_mem_block *call, *response;
    xmlrpc_value *value;

    /* Build a call, and tell the registry to handle it. */
    call = xmlrpc_mem_block_new(env, 0);
    TEST_NO_FAULT(env);
    xmlrpc_serialize_call(env, call, method_name, arg_array);
    TEST_NO_FAULT(env);
    response = xmlrpc_registry_process_call(env, registry, NULL,
                                            xmlrpc_mem_block_contents(call),
                                            xmlrpc_mem_block_size(call));
    TEST_NO_FAULT(env);
    TEST(response != NULL);

    /* Parse the response. */
    value = xmlrpc_parse_response(env, xmlrpc_mem_block_contents(response),
                                  xmlrpc_mem_block_size(response));

    xmlrpc_mem_block_free(call);
    xmlrpc_mem_block_free(response);
    return value;
}

static void test_method_registry (void)
{
    xmlrpc_env env, env2;
    xmlrpc_value *arg_array, *value;
    xmlrpc_registry *registry;
    xmlrpc_mem_block *response;
    xmlrpc_int32 i;

    xmlrpc_value *multi;
    xmlrpc_int32 foo1_result, foo2_result;
    xmlrpc_int32 bar_code, nosuch_code, multi_code, bogus1_code, bogus2_code;
    char *bar_string, *nosuch_string, *multi_string;
    char *bogus1_string, *bogus2_string;

    xmlrpc_env_init(&env);

    /* Create a new registry. */
    registry = xmlrpc_registry_new(&env);
    TEST(registry != NULL);
    TEST_NO_FAULT(&env);

    /* Add some test methods. */
    xmlrpc_registry_add_method(&env, registry, NULL, "test.foo",
                               test_foo, FOO_USER_DATA);
    TEST_NO_FAULT(&env);
    xmlrpc_registry_add_method(&env, registry, NULL, "test.bar",
                               test_bar, BAR_USER_DATA);
    TEST_NO_FAULT(&env);

    /* Build an argument array for our calls. */
    arg_array = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 
    TEST_NO_FAULT(&env);

    /* Call test.foo and check the result. */
    value = process_call_helper(&env, registry, "test.foo", arg_array);
    TEST_NO_FAULT(&env);
    TEST(value != NULL);
    xmlrpc_parse_value(&env, value, "i", &i);
    TEST_NO_FAULT(&env);
    TEST(i == 42);
    xmlrpc_DECREF(value);

    /* Call test.bar and check the result. */
    xmlrpc_env_init(&env2);
    value = process_call_helper(&env2, registry, "test.bar", arg_array);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == 123);
    TEST(env2.fault_string && strcmp(env2.fault_string, "Test fault") == 0);
    xmlrpc_env_clean(&env2);

    /* Call a non-existant method and check the result. */
    xmlrpc_env_init(&env2);
    value = process_call_helper(&env2, registry, "test.nosuch", arg_array);
    TEST(value == NULL);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_NO_SUCH_METHOD_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test system.multicall. */
    multi = xmlrpc_build_value(&env,
                               "(({s:s,s:V}{s:s,s:V}{s:s,s:V}"
                               "{s:s,s:()}s{}{s:s,s:V}))",
                               "methodName", "test.foo",
                               "params", arg_array,
                               "methodName", "test.bar",
                               "params", arg_array,
                               "methodName", "test.nosuch",
                               "params", arg_array,
                               "methodName", "system.multicall",
                               "params",
                               "bogus_entry",
                               "methodName", "test.foo",
                               "params", arg_array);
    TEST_NO_FAULT(&env);    
    value = process_call_helper(&env, registry, "system.multicall", multi);
    TEST_NO_FAULT(&env);
    xmlrpc_parse_value(&env, value,
                       "((i){s:i,s:s,*}{s:i,s:s,*}"
                       "{s:i,s:s,*}{s:i,s:s,*}{s:i,s:s,*}(i))",
                       &foo1_result,
                       "faultCode", &bar_code,
                       "faultString", &bar_string,
                       "faultCode", &nosuch_code,
                       "faultString", &nosuch_string,
                       "faultCode", &multi_code,
                       "faultString", &multi_string,
                       "faultCode", &bogus1_code,
                       "faultString", &bogus1_string,
                       "faultCode", &bogus2_code,
                       "faultString", &bogus2_string,
                       &foo2_result);
    TEST_NO_FAULT(&env);    
    TEST(foo1_result == 42);
    TEST(bar_code == 123);
    TEST(strcmp(bar_string, "Test fault") == 0);
    TEST(nosuch_code == XMLRPC_NO_SUCH_METHOD_ERROR);
    TEST(multi_code == XMLRPC_REQUEST_REFUSED_ERROR);
    TEST(foo2_result == 42);
    xmlrpc_DECREF(multi);
    xmlrpc_DECREF(value);

    /* PASS bogus XML data and make sure our parser pukes gracefully.
    ** (Because of the way the code is laid out, and the presence of other
    ** test suites, this lets us skip tests for invalid XML-RPC data.) */
    xmlrpc_env_init(&env2);
    response = xmlrpc_registry_process_call(&env, registry, NULL,
                                            expat_error_data,
                                            strlen(expat_error_data));
    TEST_NO_FAULT(&env);
    TEST(response != NULL);
    value = xmlrpc_parse_response(&env2, xmlrpc_mem_block_contents(response),
                                  xmlrpc_mem_block_size(response));
    TEST(value == NULL);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == XMLRPC_PARSE_ERROR);
    xmlrpc_mem_block_free(response);
    xmlrpc_env_clean(&env2);

    /* Test default method support. */
    xmlrpc_registry_set_default_method(&env, registry, &test_default,
                                       FOO_USER_DATA);
    TEST_NO_FAULT(&env);
    value = process_call_helper(&env, registry, "test.nosuch", arg_array);
    TEST_NO_FAULT(&env);
    TEST(value != NULL);
    xmlrpc_parse_value(&env, value, "i", &i);
    TEST_NO_FAULT(&env);
    TEST(i == 84);
    xmlrpc_DECREF(value);

    /* Change the default method. */
    xmlrpc_registry_set_default_method(&env, registry, &test_default,
                                       BAR_USER_DATA);
    TEST_NO_FAULT(&env);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_registry_free(registry);
    xmlrpc_DECREF(arg_array);

    xmlrpc_env_clean(&env);
}

static void test_nesting_limit (void)
{
    xmlrpc_env env;
    xmlrpc_value *val;

    xmlrpc_env_init(&env);
    
    /* Test with an adequate limit for (...(...()...)...). */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 2);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST_NO_FAULT(&env);
    TEST(val != NULL);
    xmlrpc_DECREF(val);

    /* Test with an inadequate limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 1);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_PARSE_ERROR); /* BREAKME - Will change. */
    TEST(val == NULL);

    /* Reset the default limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, XMLRPC_NESTING_LIMIT_DEFAULT);
    TEST(xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID)
         == XMLRPC_NESTING_LIMIT_DEFAULT);

    xmlrpc_env_clean(&env);
}

static void test_xml_size_limit (void)
{
    xmlrpc_env env;
    char *method_name;
    xmlrpc_value *params, *val;
    

    /* NOTE - This test suite only verifies the last-ditch size-checking
    ** code.  There should also be matching code in all server (and
    ** preferably all client) modules as well. */

    /* Set our XML size limit to something ridiculous. */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 6);
    
    /* Attempt to parse a call. */
    xmlrpc_env_init(&env);
    xmlrpc_parse_call(&env, serialized_call, strlen(serialized_call),
                      &method_name, &params);
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_LIMIT_EXCEEDED_ERROR);
    TEST(method_name == NULL);
    TEST(params == NULL);
    xmlrpc_env_clean(&env);

    /* Attempt to parse a response. */
    xmlrpc_env_init(&env);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST(env.fault_occurred);
    TEST(env.fault_code == XMLRPC_LIMIT_EXCEEDED_ERROR);
    TEST(val == NULL);
    xmlrpc_env_clean(&env);

    /* Reset the default limit. */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, XMLRPC_XML_SIZE_LIMIT_DEFAULT);
}


/*=========================================================================
**  test_sample_files
**=========================================================================
**  Read in a bunch of sample test files and make sure we get plausible
**  results.
**
**  We use these files to test strange-but-legal encodings, illegal-but-
**  supported encodings, etc.
*/

static char *good_requests[] = {
    TESTDATADIR DIRECTORY_SEPARATOR "req_out_of_order.xml",
    TESTDATADIR DIRECTORY_SEPARATOR "req_no_params.xml",
    TESTDATADIR DIRECTORY_SEPARATOR "req_value_name.xml",
    NULL
};

#define MAX_SAMPLE_FILE_LEN (16 * 1024)

static char file_buff [MAX_SAMPLE_FILE_LEN];

static void
read_file (char *path, char **out_data, size_t *out_size)
{
    FILE *f;
    size_t bytes_read;

    /* Open the file. */
    f = fopen(path, "r");
    if (f == NULL) {
        /* Since this error is fairly likely to happen, give an
        ** informative error message... */
        fflush(stdout);
        fprintf(stderr, "Could not open file '%s'.  errno=%d (%s)\n", 
                path, errno, strerror(errno));
        exit(1);
    }
    
    /* Read in one buffer full of data, and make sure that everything
    ** fit.  (We perform a lazy error/no-eof/zero-length-file test using
    ** bytes_read.) */
    bytes_read = fread(file_buff, sizeof(char), MAX_SAMPLE_FILE_LEN, f);
    TEST(0 < bytes_read && bytes_read < MAX_SAMPLE_FILE_LEN);

    /* Close the file and return our data. */
    fclose(f);
    *out_data = file_buff;
    *out_size = bytes_read;
}

static void test_sample_files (void)
{
    xmlrpc_env env;
    char **paths, *path;
    char *data;
    size_t data_len;
    char *method_name;
    xmlrpc_value *params;

    xmlrpc_env_init(&env);

    /* Test our good requests. */
    for (paths = good_requests; *paths != NULL; paths++) {
        path = *paths;
        read_file(path, &data, &data_len);
        xmlrpc_parse_call(&env, data, data_len, &method_name, &params);
        TEST_NO_FAULT(&env);
        free(method_name);
        xmlrpc_DECREF(params);
    }

    xmlrpc_env_clean(&env);
}


/*=========================================================================
**  test_utf8_coding
**=========================================================================
**  We need to test our UTF-8 decoder thoroughly.  Most of these test
**  cases are taken from the UTF-8-test.txt file by Markus Kuhn
**  <mkuhn@acm.org>:
**      http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
*/

#ifdef HAVE_UNICODE_WCHAR

typedef struct {
    char *utf8;
    wchar_t wcs[16];
} utf8_and_wcs;

static utf8_and_wcs good_utf8[] = {

    /* Greek 'kosme'. */
    {"\316\272\341\275\271\317\203\316\274\316\265",
     {0x03BA, 0x1F79, 0x03C3, 0x03BC, 0x03B5, 0}},

    /* First sequences of a given length. */
    /* '\000' is not a legal C string. */
    {"\302\200", {0x0080, 0}},
    {"\340\240\200", {0x0800, 0}},

    /* Last sequences of a given length. */
    {"\177", {0x007F, 0}},
    {"\337\277", {0x07FF, 0}},
    /* 0xFFFF is not a legal Unicode character. */

    /* Other boundry conditions. */
    {"\001", {0x0001, 0}},
    {"\355\237\277", {0xD7FF, 0}},
    {"\356\200\200", {0xE000, 0}},
    {"\357\277\275", {0xFFFD, 0}},

    /* Other random test cases. */
    {"", {0}},
    {"abc", {0x0061, 0x0062, 0x0063, 0}},
    {"[\302\251]", {0x005B, 0x00A9, 0x005D, 0}},
    
    {NULL, {0}}
};

static char *(bad_utf8[]) = {

    /* Continuation bytes. */
    "\200", "\277",

    /* Lonely start characters. */
    "\300", "\300x", "\300xx",
    "\340", "\340x", "\340xx", "\340xxx",

    /* Last byte missing. */
    "\340\200", "\340\200x", "\340\200xx",
    "\357\277", "\357\277x", "\357\277xx",

    /* Illegal bytes. */
    "\376", "\377",

    /* Overlong '/'. */
    "\300\257", "\340\200\257",

    /* Overlong ASCII NUL. */
    "\300\200", "\340\200\200",

    /* Maximum overlong sequences. */
    "\301\277", "\340\237\277",

    /* Illegal code positions. */
    "\357\277\276", /* U+FFFE */
    "\357\277\277", /* U+FFFF */

    /* UTF-16 surrogates (unpaired and paired). */
    "\355\240\200",
    "\355\277\277",
    "\355\240\200\355\260\200",
    "\355\257\277\355\277\277",

    /* Valid UCS-4 characters (not supported yet).
    ** On systems with UCS-4 or UTF-16 wchar_t values, these
    ** may eventually be supported in some fashion. */
    "\360\220\200\200",
    "\370\210\200\200\200",
    "\374\204\200\200\200\200",

    NULL
};

/* This routine is missing on certain platforms.  This implementation
** *appears* to be correct. */
#if 0
#ifndef HAVE_WCSNCMP
int wcsncmp(wchar_t *wcs1, wchar_t* wcs2, size_t len)
{
    size_t i;
    /* XXX - 'unsigned long' should be 'uwchar_t'. */
    unsigned long c1, c2;
    for (i=0; i < len; i++) {
        c1 = wcs1[i];
        c2 = wcs2[i];
        /* This clever comparison borrowed from the GNU C Library. */
        if (c1 == 0 || c1 != c2)
            return c1 - c2;
    }
    return 0;
}
#endif /* HAVE_WCSNCMP */
#endif

static void test_utf8_coding (void)
{
    xmlrpc_env env, env2;
    utf8_and_wcs *good_data;
    char **bad_data;
    char *utf8;
    wchar_t *wcs;
    xmlrpc_mem_block *output;

    xmlrpc_env_init(&env);

    /* Test each of our valid UTF-8 sequences. */
    for (good_data = good_utf8; good_data->utf8 != NULL; good_data++) {
        utf8 = good_data->utf8;
        wcs = good_data->wcs;

        /* Attempt to validate the UTF-8 string. */
        xmlrpc_validate_utf8(&env, utf8, strlen(utf8));
        TEST_NO_FAULT(&env);

        /* Attempt to decode the UTF-8 string. */
        output = xmlrpc_utf8_to_wcs(&env, utf8, strlen(utf8));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(wcslen(wcs) == XMLRPC_TYPED_MEM_BLOCK_SIZE(wchar_t, output));
        TEST(0 ==
             wcsncmp(wcs, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(wchar_t, output),
                     wcslen(wcs)));
        xmlrpc_mem_block_free(output);

        /* Test the UTF-8 encoder, too. */
        output = xmlrpc_wcs_to_utf8(&env, wcs, wcslen(wcs));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(strlen(utf8) == XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output));
        TEST(0 ==
             strncmp(utf8, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                     strlen(utf8)));
        xmlrpc_mem_block_free(output);
    }

    /* Test each of our illegal UTF-8 sequences. */
    for (bad_data = bad_utf8; *bad_data != NULL; bad_data++) {
        utf8 = *bad_data;
    
        /* Attempt to validate the UTF-8 string. */
        xmlrpc_env_init(&env2);
        xmlrpc_validate_utf8(&env2, utf8, strlen(utf8));
        TEST(env2.fault_occurred);
        TEST(env2.fault_code == XMLRPC_INVALID_UTF8_ERROR);
        /* printf("Fault: %s\n", env2.fault_string); --Hand-checked */
        xmlrpc_env_clean(&env2);

        /* Attempt to decode the UTF-8 string. */
        xmlrpc_env_init(&env2);
        output = xmlrpc_utf8_to_wcs(&env2, utf8, strlen(utf8));
        TEST(env2.fault_occurred);
        TEST(env2.fault_code == XMLRPC_INVALID_UTF8_ERROR);
        TEST(output == NULL);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_env_clean(&env);
}

static char utf8_data[] = "[\302\251\0]";
static wchar_t wcs_data[] = {0x005B, 0x00A9, 0, 0x005D, 0};

static void test_wchar_support (void)
{
    xmlrpc_env env;
    xmlrpc_value *val;
    wchar_t *wcs;
    char *str;
    size_t len;

    xmlrpc_env_init(&env);

    /* Build a string from UTF-8 data. */
    val = xmlrpc_build_value(&env, "s#", utf8_data, (size_t) 5);
    TEST_NO_FAULT(&env);
    TEST(val != NULL);

    /* Extract it as a wchar_t string. */
    xmlrpc_parse_value(&env, val, "w#", &wcs, &len);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 4);
    TEST(wcs[len] == '\0');
    TEST(0 == wcsncmp(wcs, wcs_data, len));
    xmlrpc_DECREF(val);

    /* Build a string from wchar_t data. */
    val = xmlrpc_build_value(&env, "w#", wcs_data, 4);
    TEST_NO_FAULT(&env);
    TEST(val != NULL);

    /* Extract it as a wchar_t string. */
    xmlrpc_parse_value(&env, val, "w#", &wcs, &len);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 4);
    TEST(wcs[len] == '\0');
    TEST(0 == wcsncmp(wcs, wcs_data, len));

    /* Extract it as a UTF-8 string. */
    xmlrpc_parse_value(&env, val, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(str != NULL);
    TEST(len == 5);
    TEST(str[len] == '\0');
    TEST(0 == strncmp(str, utf8_data, len));
    xmlrpc_DECREF(val);
    
    xmlrpc_env_clean(&env);
}

#endif /* HAVE_UNICODE_WCHAR */


/*=========================================================================
**  Test Driver
**=========================================================================
*/

int 
main(int     argc, 
     char ** argv ATTR_UNUSED) {

    if (argc-1 > 0) {
        fprintf(stderr, "There are no arguments.");
        exit(1);
    }

    /* Add your test suites here. */
    test_env();
    test_mem_block();
    test_base64_conversion();
    test_value();
    test_bounds_checks();
    test_struct();
    test_serialize();
    test_expat();
    test_parse_xml_value();
    test_parse_xml_response();
    test_parse_xml_call();
    test_method_registry();
    test_nesting_limit();
    test_xml_size_limit();
    test_sample_files();

#ifdef HAVE_UNICODE_WCHAR
    test_utf8_coding();
    test_wchar_support();
#endif /* HAVE_UNICODE_WCHAR */

    /* Summarize our test run. */
    printf("\nRan %d tests, %d failed, %.1f%% passed\n",
           total_tests, total_failures,
           100.0 - (100.0 * total_failures) / total_tests);

    /* Print the final result. */
    if (total_failures == 0) {
        printf("OK\n");
        return 0;
    }

    printf("FAILED\n");
    return 1;
}

#include <string.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"

#include "testtool.h"


#include "memblock.h"



static char* test_string_1 = "foo";
static int test_int_array_1[5] = {1, 2, 3, 4, 5};
static int test_int_array_2[3] = {6, 7, 8};
static int test_int_array_3[8] = {1, 2, 3, 4, 5, 6, 7, 8};



static void
testMemBlock(void) {

    xmlrpc_env env;
    xmlrpc_mem_block * blockP;

    xmlrpc_mem_block * typedHeapBlockP;
    void ** typedContents;

    xmlrpc_env_init(&env);

    /* Allocate a zero-size block. */
    blockP = xmlrpc_mem_block_new(&env, 0);
    TEST_NO_FAULT(&env);
    TEST(blockP != NULL);
    TEST(xmlrpc_mem_block_size(blockP) == 0);

    /* Grow the block a little bit. */
    xmlrpc_mem_block_resize(&env, blockP, strlen(test_string_1) + 1);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(blockP) == strlen(test_string_1) + 1);
    
    /* Insert a string into the block, and resize it by large amount.
    ** We want to cause a reallocation and copy of the block contents. */
    strcpy(xmlrpc_mem_block_contents(blockP), test_string_1);
    xmlrpc_mem_block_resize(&env, blockP, 10000);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(blockP) == 10000);
    TEST(xmlrpc_streq(xmlrpc_mem_block_contents(blockP), test_string_1));

    /* Test growing beyond a megabyte */
    xmlrpc_mem_block_resize(&env, blockP, 1024*1024+1);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(blockP) == 1024*1024+1);
    TEST(xmlrpc_streq(xmlrpc_mem_block_contents(blockP), test_string_1));

    /* Test shrinking */
    xmlrpc_mem_block_resize(&env, blockP, 2);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(blockP) == 2);
    TEST(xmlrpc_memeq(xmlrpc_mem_block_contents(blockP), test_string_1, 2));

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(blockP);
    
    /* Allocate a bigger block. */
    blockP = xmlrpc_mem_block_new(&env, 128);
    TEST_NO_FAULT(&env);
    TEST(blockP != NULL);
    TEST(xmlrpc_mem_block_size(blockP) == 128);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(blockP);

    /* Allocate a "typed" memory block. */
    typedHeapBlockP = XMLRPC_TYPED_MEM_BLOCK_NEW(void *, &env, 20);
    TEST_NO_FAULT(&env);
    TEST(typedHeapBlockP != NULL);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typedHeapBlockP) == 20);
    typedContents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(void*, typedHeapBlockP);
    TEST(typedContents != NULL);

    /* Resize a typed memory block. */
    XMLRPC_TYPED_MEM_BLOCK_RESIZE(void*, &env, typedHeapBlockP, 100);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typedHeapBlockP) == 100);

    /* Test cleanup code (with help from memprof). */
    XMLRPC_TYPED_MEM_BLOCK_FREE(void*, typedHeapBlockP);

    /* Test xmlrpc_mem_block_append. */
    blockP = XMLRPC_TYPED_MEM_BLOCK_NEW(int, &env, 5);
    TEST_NO_FAULT(&env);
    memcpy(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, blockP),
           test_int_array_1, sizeof(test_int_array_1));
    XMLRPC_TYPED_MEM_BLOCK_APPEND(int, &env, blockP, test_int_array_2, 3);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(int, blockP) == 8);
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, blockP),
                test_int_array_3, sizeof(test_int_array_3)) == 0);
    XMLRPC_TYPED_MEM_BLOCK_FREE(int, blockP);

    xmlrpc_env_clean(&env);
}



static void
testMemPool(void) {

    xmlrpc_env env;

    xmlrpc_mem_pool * poolP;

    xmlrpc_env_init(&env);

    poolP = xmlrpc_mem_pool_new(&env, 1);
    TEST_NO_FAULT(&env);
    TEST(poolP != NULL);

    xmlrpc_mem_pool_free(poolP);

    poolP = xmlrpc_mem_pool_new(&env, 10);
    TEST_NO_FAULT(&env);
    TEST(poolP != NULL);
    
    xmlrpc_mem_pool_alloc(&env, poolP, 1);
    TEST_NO_FAULT(&env);

    xmlrpc_mem_pool_release(poolP, 1);
    TEST_NO_FAULT(&env);

    xmlrpc_mem_pool_alloc(&env, poolP, 5);
    TEST_NO_FAULT(&env);

    {
        xmlrpc_env env2;
        xmlrpc_env_init(&env2);
        xmlrpc_mem_pool_alloc(&env2, poolP, 6);
        TEST_FAULT(&env2, XMLRPC_LIMIT_EXCEEDED_ERROR);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_mem_pool_alloc(&env, poolP, 5);
    TEST_NO_FAULT(&env);

    xmlrpc_mem_pool_release(poolP, 5);
    TEST_NO_FAULT(&env);
    
    xmlrpc_mem_pool_release(poolP, 5);
    TEST_NO_FAULT(&env);

    xmlrpc_mem_pool_free(poolP);

    xmlrpc_env_clean(&env);
}



static void
testMemBlockPool(void) {

    xmlrpc_env env;

    xmlrpc_mem_pool * poolP;
    xmlrpc_mem_block * block1P;

    xmlrpc_env_init(&env);

    /* The memory block facility often allocates more memory that we request,
       anticipating the future (e.g. if we create a 1-byte block, it allocates
       16), so we work in large numbers.
    */

    poolP = xmlrpc_mem_pool_new(&env, 1000);
    TEST_NO_FAULT(&env);
    TEST(poolP != NULL);
    
    block1P = xmlrpc_mem_block_new_pool(&env, 1000, poolP);
    TEST_NO_FAULT(&env);
    TEST(block1P != NULL);

    xmlrpc_mem_block_free(block1P);

    block1P = xmlrpc_mem_block_new_pool(&env, 200, poolP);

    {
        xmlrpc_env env2;
        xmlrpc_mem_block * blockP;
        xmlrpc_env_init(&env2);
        blockP = xmlrpc_mem_block_new_pool(&env2, 900, poolP);
        TEST_FAULT(&env2, XMLRPC_LIMIT_EXCEEDED_ERROR);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_mem_block_resize(&env, block1P, 500);
    TEST_NO_FAULT(&env);

    {
        xmlrpc_env env2;
        xmlrpc_env_init(&env2);
        xmlrpc_mem_block_resize(&env2, block1P, 1100);
        TEST_FAULT(&env2, XMLRPC_LIMIT_EXCEEDED_ERROR);
        xmlrpc_env_clean(&env2);
    }

    {
        xmlrpc_env env2;
        xmlrpc_mem_block * blockP;
        xmlrpc_env_init(&env2);
        blockP = xmlrpc_mem_block_new_pool(&env2, 600, poolP);
        TEST_FAULT(&env2, XMLRPC_LIMIT_EXCEEDED_ERROR);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_mem_block_free(block1P);

    xmlrpc_mem_pool_free(poolP);

    xmlrpc_env_clean(&env);
}



void
test_memBlock() {

    printf("Running memory manager tests...");
    
    testMemBlock();

    testMemPool();

    testMemBlockPool();

    printf("\n");
    printf("Memory manager tests done.\n");
}



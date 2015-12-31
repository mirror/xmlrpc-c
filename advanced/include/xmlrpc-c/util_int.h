#ifndef XMLRPC_C_UTIL_INT_H_INCLUDED
#define XMLRPC_C_UTIL_INT_H_INCLUDED

/* This file contains facilities for use by Xmlrpc-c code, but not intended
   to be included in a user compilation.

   Names in here might conflict with other names in a user's compilation
   if included in a user compilation.

   The facilities may change in future releases.
*/

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* When we deallocate a pointer in a struct, we often replace it with
** this and throw in a few assertions here and there. */
#define XMLRPC_BAD_POINTER ((void*) 0xDEADBEEF)

/*============================================================================
  xmlrpc_mem_pool

  A memory pool from which you can allocate xmlrpc_mem_block's.

  This is a mechanism for limiting memory allocation.
============================================================================*/

typedef struct _xmlrpc_mem_pool xmlrpc_mem_pool;

XMLRPC_UTIL_EXPORTED
xmlrpc_mem_pool *
xmlrpc_mem_pool_new(xmlrpc_env * const envP,
                    size_t       const size);

XMLRPC_UTIL_EXPORTED
void
xmlrpc_mem_pool_free(xmlrpc_mem_pool * const poolP);

void
xmlrpc_mem_pool_alloc(xmlrpc_env *      const envP, 
                      xmlrpc_mem_pool * const poolP,
                      size_t            const size);

void
xmlrpc_mem_pool_release(xmlrpc_mem_pool * const poolP,
                        size_t            const size);

/*============================================================================
  xmlrpc_mem_block

  A resizable chunk of memory. This is mostly used internally, but it is
  also used by the public API in a few places.
============================================================================*/

/* Allocate a new xmlrpc_mem_block. */
XMLRPC_UTIL_EXPORTED
xmlrpc_mem_block *
xmlrpc_mem_block_new(xmlrpc_env * const envP,
                     size_t       const size);

XMLRPC_UTIL_EXPORTED
xmlrpc_mem_block *
xmlrpc_mem_block_new_pool(xmlrpc_env *      const envP,
                          size_t            const size,
                          xmlrpc_mem_pool * const poolP);
    
/* Destroy an existing xmlrpc_mem_block, and everything it contains. */
XMLRPC_UTIL_EXPORTED
void
xmlrpc_mem_block_free(xmlrpc_mem_block * const blockP);

/* Get the size and contents of the xmlrpc_mem_block. */
XMLRPC_UTIL_EXPORTED
size_t 
xmlrpc_mem_block_size(const xmlrpc_mem_block * const block);

XMLRPC_UTIL_EXPORTED
void * 
xmlrpc_mem_block_contents(const xmlrpc_mem_block * const block);

/* Resize an xmlrpc_mem_block, preserving as much of the contents as possible.
*/
XMLRPC_UTIL_EXPORTED
void
xmlrpc_mem_block_resize(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const blockP,
                        size_t             const size);

/* Append data to an existing xmlrpc_mem_block. */
XMLRPC_UTIL_EXPORTED
void xmlrpc_mem_block_append(xmlrpc_env *       const envP,
                             xmlrpc_mem_block * const blockP,
                             const void *       const data,
                             size_t             const len);

#define XMLRPC_MEMBLOCK_NEW(type,env,size) \
    xmlrpc_mem_block_new((env), sizeof(type) * (size))
#define XMLRPC_MEMBLOCK_FREE(type,block) \
    xmlrpc_mem_block_free(block)
#define XMLRPC_MEMBLOCK_SIZE(type,block) \
    (xmlrpc_mem_block_size(block) / sizeof(type))
#define XMLRPC_MEMBLOCK_CONTENTS(type,block) \
    ((type*) xmlrpc_mem_block_contents(block))
#define XMLRPC_MEMBLOCK_RESIZE(type,env,block,size) \
    xmlrpc_mem_block_resize(env, block, sizeof(type) * (size))
#define XMLRPC_MEMBLOCK_APPEND(type,env,block,data,size) \
    xmlrpc_mem_block_append(env, block, data, sizeof(type) * (size))

/* Here are some backward compatibility definitions.  These longer names
   used to be the only ones and typed memory blocks were considered
   special.
*/
#define XMLRPC_TYPED_MEM_BLOCK_NEW(type,env,size) \
    XMLRPC_MEMBLOCK_NEW(type,env,size)
#define XMLRPC_TYPED_MEM_BLOCK_FREE(type,block) \
    XMLRPC_MEMBLOCK_FREE(type,block)
#define XMLRPC_TYPED_MEM_BLOCK_SIZE(type,block) \
    XMLRPC_MEMBLOCK_SIZE(type,block)
#define XMLRPC_TYPED_MEM_BLOCK_CONTENTS(type,block) \
    XMLRPC_MEMBLOCK_CONTENTS(type,block)
#define XMLRPC_TYPED_MEM_BLOCK_RESIZE(type,env,block,size) \
    XMLRPC_MEMBLOCK_RESIZE(type,env,block,size)
#define XMLRPC_TYPED_MEM_BLOCK_APPEND(type,env,block,data,size) \
    XMLRPC_MEMBLOCK_APPEND(type,env,block,data,size)


#ifdef __cplusplus
}
#endif

#endif

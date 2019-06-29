/* Copyright information is at end of file */
#include "xmlrpc_config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "bool.h"
#include "girmath.h"
#include "mallocvar.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/util.h"

#define BLOCK_ALLOC_MIN (16)

static bool const tracingMemory =
#ifdef EFENCE
    true
#else
    false
#endif
    ;



struct _xmlrpc_mem_block {
    xmlrpc_mem_pool * poolP;
        /* Pool this block is in; NULL if it is not in a pool */
    size_t size;
        /* Size of the block */
    size_t allocated;
        /* How much memory we've allocated from the system for this block
           (pointed to by 'blockP')
        */
    void * blockP;
};



xmlrpc_mem_block *
xmlrpc_mem_block_new_pool(xmlrpc_env *      const envP,
                          size_t            const size,
                          xmlrpc_mem_pool * const poolP) {
/*----------------------------------------------------------------------------
   Create an xmlrpc_mem_block of size 'size' in pool *poolP.

   If 'poolP' is NULL, don't put it in any pool.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * blockP;

    XMLRPC_ASSERT_ENV_OK(envP);

    if (!envP->fault_occurred) {
        MALLOCVAR(blockP);
    
        if (blockP == NULL)
            xmlrpc_faultf(envP, "Can't allocate memory block descriptor");
        else {
            blockP->poolP = poolP;

            blockP->size = size;

            if (tracingMemory)
                blockP->allocated = size;
            else
                blockP->allocated = MAX(BLOCK_ALLOC_MIN, size);
            
            if (poolP)
                xmlrpc_mem_pool_alloc(envP, poolP, blockP->allocated);

            if (!envP->fault_occurred) {
                blockP->blockP = malloc(blockP->allocated);
                if (!blockP->blockP)
                    xmlrpc_faultf(envP, "Can't allocate %u-byte memory block",
                                  (unsigned)blockP->allocated);
                

                if (envP->fault_occurred)
                    xmlrpc_mem_pool_release(poolP, blockP->allocated);
            }
            if (envP->fault_occurred) {
                free(blockP);
                blockP = NULL;
            }
        }
    }
    return blockP;
}



xmlrpc_mem_block * 
xmlrpc_mem_block_new(xmlrpc_env * const envP, 
                     size_t       const size) {
/*----------------------------------------------------------------------------
   Create an xmlrpc_mem_block of size 'size', not in any pool
-----------------------------------------------------------------------------*/
    return xmlrpc_mem_block_new_pool(envP, size, NULL);
}



void
xmlrpc_mem_block_free(xmlrpc_mem_block * const blockP) {
/*----------------------------------------------------------------------------
   Destroy xmlrpc_mem_block *blockP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(blockP != NULL);
    XMLRPC_ASSERT(blockP->blockP != NULL);

    if (blockP->poolP)
        xmlrpc_mem_pool_release(blockP->poolP, blockP->allocated);

    free(blockP->blockP);

    free(blockP);
}



size_t 
xmlrpc_mem_block_size(const xmlrpc_mem_block * const blockP) {
/*----------------------------------------------------------------------------
   The size of the xmlrpc_mem_block.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(blockP != NULL);
    return blockP->size;
}



void * 
xmlrpc_mem_block_contents(const xmlrpc_mem_block * const blockP) {
/*----------------------------------------------------------------------------
   The contents of the xmlrpc_mem_block
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(blockP != NULL);
    return blockP->blockP;
}



static size_t
allocSize(size_t const requestedSize) {
/*----------------------------------------------------------------------------
   The size we allocate when the user requests to resize to 'requesteddSize'.

   We give him more than he requested in an attempt to avoid lots of copying
   when Caller repeatedly resizes by small amounts.

   We make some arbitrary judgments about how much memory or copying
   constitutes lots.
-----------------------------------------------------------------------------*/
    size_t const oneMegabyte = 1024*1024;

    size_t retval;

    if (tracingMemory)
        retval = requestedSize;
    else {
        /* We make it a power of two unless it is more than a megabyte,
           in which case we just make it a multiple of a megabyte.
        */
        if (requestedSize >= oneMegabyte)
            retval = ROUNDUPU(requestedSize, oneMegabyte);
        else {
            unsigned int i;
            for (i = BLOCK_ALLOC_MIN; i < requestedSize; i *= 2);
            retval = i;
        }
    }
    return retval;
}



void 
xmlrpc_mem_block_resize(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const blockP,
                        size_t             const size) {
/*----------------------------------------------------------------------------
  Resize an xmlrpc_mem_block by allocating new memory for it and copying
  whatever might be in it to the new memory.
-----------------------------------------------------------------------------*/
    size_t const newAllocSize = allocSize(size);

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(blockP != NULL);

    if (newAllocSize > blockP->allocated) {
        /* Reallocate */

        if (blockP->poolP)
            xmlrpc_mem_pool_alloc(envP, blockP->poolP,
                                  newAllocSize - blockP->allocated);

        if (!envP->fault_occurred) {
            void * newMem;

            newMem = malloc(newAllocSize);
            if (!newMem)
                xmlrpc_faultf(envP, 
                              "Failed to allocate %u bytes of memory "
                              "from the OS",
                              (unsigned) size);
            else {
                /* Copy over the data */
                size_t const sizeToCopy = MIN(blockP->size, size);
                assert(sizeToCopy <= newAllocSize);
                memcpy(newMem, blockP->blockP, sizeToCopy);
                
                free(blockP->blockP);
                
                blockP->blockP    = newMem;
                blockP->allocated = newAllocSize;
            }
            if (envP->fault_occurred)
                xmlrpc_mem_pool_release(blockP->poolP,
                                        newAllocSize - blockP->allocated);
        }
    }
    blockP->size = size;
}



void 
xmlrpc_mem_block_append(xmlrpc_env *       const envP,
                        xmlrpc_mem_block * const blockP,
                        const void *       const data, 
                        size_t             const len) {

    size_t const originalSize = blockP->size;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(blockP != NULL);

    xmlrpc_mem_block_resize(envP, blockP, originalSize + len);
    if (!envP->fault_occurred) {
        memcpy(((unsigned char*) blockP->blockP) + originalSize, data, len);
    }
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
** SUCH DAMAGE.
*/

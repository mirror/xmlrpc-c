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

struct _xmlrpc_mem_pool {
    size_t       size;
    size_t       allocated;
};



xmlrpc_mem_pool * 
xmlrpc_mem_pool_new(xmlrpc_env * const envP, 
                    size_t       const size) {
/*----------------------------------------------------------------------------
   Create an xmlrpc_mem_pool of size 'size' bytes.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_pool * poolP;

    XMLRPC_ASSERT_ENV_OK(envP);

    MALLOCVAR(poolP);
    
    if (poolP == NULL)
        xmlrpc_faultf(envP, "Can't allocate memory pool descriptor");
    else {
        poolP->size = size;

        poolP->allocated = 0;
    
        if (envP->fault_occurred)
            free(poolP);
    }
    return poolP;
}



void
xmlrpc_mem_pool_free(xmlrpc_mem_pool * const poolP) {
/*----------------------------------------------------------------------------
   Destroy xmlrpc_mem_pool *poolP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(poolP != NULL);

    XMLRPC_ASSERT(poolP->allocated == 0);

    free(poolP);
}



void
xmlrpc_mem_pool_alloc(xmlrpc_env *      const envP, 
                      xmlrpc_mem_pool * const poolP,
                      size_t            const size) {
/*----------------------------------------------------------------------------
   Take 'size' bytes from pool *poolP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(poolP->allocated <= poolP->size);

    if (poolP->size - poolP->allocated < size)
        xmlrpc_faultf(envP, "Memory pool is out of memory.  "
                      "%u-byte pool is %u bytes short",
                      (unsigned)poolP->size,
                      (unsigned)(poolP->allocated + size - poolP->size));
    else
        poolP->allocated += size;
}


                      
void
xmlrpc_mem_pool_release(xmlrpc_mem_pool * const poolP,
                        size_t            const size) {
/*----------------------------------------------------------------------------
   Return 'size' bytes to pool *poolP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(poolP != NULL);

    XMLRPC_ASSERT(poolP->allocated >= size);

    poolP->allocated -= size;
}




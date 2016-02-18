#include "xmlrpc_config.h"
#include <assert.h>
#include <pthread.h>
#include <openssl/crypto.h>

#include "mallocvar.h"

#include "xmlrpc-c/string_int.h"

#include "xmlrpc-c/openssl_thread.h"


/* These ugly global variables are necessary and appropriate because our
   purpose is to manipulate OpenSSL's ugly global state.
*/

static pthread_mutex_t * opensslMutex;
    /* Dynamically allocated array with an entry for each lock OpenSSL
       has created.  opensslMutex[N] is the mutex for the lock known by
       OpenSSL with ID N.
    */
      
static unsigned int maxLockCt;
 


static void
lock(int          const mode,
     int          const lockId,
     const char * const fileName ATTR_UNUSED,
     int          const lineNum ATTR_UNUSED) {
/*----------------------------------------------------------------------------
   This is a locking function for OpenSSL to call when it wants to lock or
   unlock a lock.
-----------------------------------------------------------------------------*/
    assert(lockId >= 0);
    assert((unsigned int)lockId < maxLockCt);

    pthread_mutex_t * const mutexP = &opensslMutex[lockId];

    if (mode & CRYPTO_LOCK)
        pthread_mutex_lock(mutexP);
    else
        pthread_mutex_unlock(mutexP);
}



static unsigned long
id(void) {
/*----------------------------------------------------------------------------
   This is an ID function for OpenSSL to call when it wants a unique
   identifier of the executing thread.
-----------------------------------------------------------------------------*/
    return ((unsigned long)pthread_self());
}



void
xmlrpc_openssl_thread_setup(const char ** const errorP) {
/*----------------------------------------------------------------------------
   Set up the OpenSSL library to handle being called by multiple threads
   concurrently.

   This setup is process-global, so this must be called exactly once in the
   program (ergo a subroutine library cannot normally call this under the
   covers; if a subroutine library want to use OpenSSL in a thread-safe
   manner, it must depend upon its user to call this).

   This needs to be called as the program starts up, when it is only one
   thread.

   (But this need not be called at all in a program that will never call
   OpenSSL functions from multiple threads).
-----------------------------------------------------------------------------*/
    maxLockCt = CRYPTO_num_locks();

    MALLOCARRAY(opensslMutex, maxLockCt);

    if (!opensslMutex)
        xmlrpc_asprintf(errorP, "Failed to allocate an array for %u "
                        "potential OpenSSL locks", maxLockCt);
    else {
        *errorP = NULL;
        
        unsigned int i;

        for (i = 0;  i < maxLockCt;  ++i)
            pthread_mutex_init(&opensslMutex[i], NULL);

        CRYPTO_set_id_callback(id);

        CRYPTO_set_locking_callback(lock);
    }
}



void
xmlrpc_openssl_thread_cleanup() {
/*----------------------------------------------------------------------------
   Undo 'xmlrpc_openssl_thread_setup'.

   This should be called at the end of a program, when there is only one
   thread left.

   (As a practical matter, there's really no need to call this at all; if the
   program is just going to exit afterward, any cleanup we do is moot).
-----------------------------------------------------------------------------*/
    CRYPTO_set_locking_callback(NULL);

    CRYPTO_set_id_callback(NULL);

    {
        unsigned int i;

        for (i = 0;  i < maxLockCt; ++i)
            pthread_mutex_destroy(&opensslMutex[i]);
    }
    free(opensslMutex);
}



/*=============================================================================
                               curlMulti
===============================================================================
   This is an extension to Curl's CURLM object.  The extensions are:

   1) It has a lock so multiple threads can use it simultaneously.

   2) Its "select" file descriptor vectors are self-contained.  CURLM
      requires the user to maintain them separately.
=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include "xmlrpc_config.h"

#include <stdlib.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <curl/curl.h>
#ifdef NEED_CURL_TYPES_H
#include <curl/types.h>
#endif
#include <curl/easy.h>
#include <curl/multi.h>

#include "mallocvar.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/lock.h"
#include "xmlrpc-c/lock_platform.h"

#include "curlversion.h"

#include "curlmulti.h"



static void
interpretCurlMultiError(const char ** const descriptionP,
                        CURLMcode     const code) {

#if HAVE_CURL_STRERROR
    *descriptionP = strdup(curl_multi_strerror(code));
#else
    xmlrpc_asprintf(descriptionP, "Curl error code (CURLMcode) %d", code);
#endif
}



struct curlMulti {
    CURLM * curlMultiP;
    struct lock * lockP;
        /* Hold this lock while accessing or using *curlMultiP.  You're
           using the multi manager whenever you're calling a Curl
           library multi manager function.
        */
};



curlMulti *
curlMulti_create(void) {

    curlMulti * retval;
    curlMulti * curlMultiP;

    MALLOCVAR(curlMultiP);

    if (curlMultiP == NULL)
        retval = NULL;
    else {
        curlMultiP->lockP = xmlrpc_lock_create();

        if (curlMultiP->lockP == NULL)
            retval = NULL;
        else {
            curlMultiP->curlMultiP = curl_multi_init();
            if (curlMultiP->curlMultiP == NULL)
                retval = NULL;
            else
                retval = curlMultiP;

            if (retval == NULL)
                curlMultiP->lockP->destroy(curlMultiP->lockP);
        }
        if (retval == NULL)
            free(curlMultiP);
    }
    return retval;
}



void
curlMulti_destroy(curlMulti * const curlMultiP) {

    curl_multi_cleanup(curlMultiP->curlMultiP);

    curlMultiP->lockP->destroy(curlMultiP->lockP);

    free(curlMultiP);
}



void
curlMulti_perform(xmlrpc_env * const envP,
                  curlMulti *  const curlMultiP,
                  int *        const runningHandleCtP) {
/*----------------------------------------------------------------------------
   Do whatever work is ready to be done under the control of multi
   manager 'curlMultiP'.  E.g. if HTTP response data has recently arrived
   from the network, process it as an HTTP response.

   Return as *runningHandleCtP the number of Curl easy handles under the
   multi manager's control that are still running -- yet to finish.
-----------------------------------------------------------------------------*/
	do
	{		
		CURLMcode rc;
		int numfds;
		
		curlMultiP->lockP->acquire(curlMultiP->lockP);
		
		rc = curl_multi_perform(curlMultiP->curlMultiP, runningHandleCtP);
		
		curlMultiP->lockP->release(curlMultiP->lockP);
		
		if (rc == CURLM_OK) {
			/* wait for activity, timeout or "nothing" */
			rc = curl_multi_wait(curlMultiP->curlMultiP, NULL, 0, 1000, numfds);
		}

		if (rc != CURLM_OK) {
			const char * reason;
            interpretCurlMultiError(&reason, rc);
            xmlrpc_faultf(envP, "Failure of curl_multi_perform(): %s", reason);
            xmlrpc_strfree(reason);
			break;
		}
		
		// Wait 100ms after timeout or no file descriptors before trying again
		if(!numfds) {
			WAITMS(100);
		}
	} while (runningHandleCtP);
}



void
curlMulti_addHandle(xmlrpc_env *       const envP,
                    curlMulti *        const curlMultiP,
                    CURL *             const curlSessionP) {

    CURLMcode rc;

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    rc = curl_multi_add_handle(curlMultiP->curlMultiP, curlSessionP);

    curlMultiP->lockP->release(curlMultiP->lockP);

    /* Old libcurl (e.g. 7.12) actually returns CURLM_CALL_MULTI_PERFORM
       (by design) when it succeeds.  Current libcurl returns CURLM_OK.
    */

    if (rc != CURLM_OK && rc != CURLM_CALL_MULTI_PERFORM) {
        const char * reason;
        interpretCurlMultiError(&reason, rc);
        xmlrpc_faultf(envP, "Could not add Curl session to the "
                      "curl multi manager.  curl_multi_add_handle() "
                      "failed: %s", reason);
        xmlrpc_strfree(reason);
    }
}



void
curlMulti_removeHandle(curlMulti *       const curlMultiP,
                       CURL *            const curlSessionP) {

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    curl_multi_remove_handle(curlMultiP->curlMultiP, curlSessionP);

    curlMultiP->lockP->release(curlMultiP->lockP);
}



void
curlMulti_getMessage(curlMulti * const curlMultiP,
                     bool *      const endOfMessagesP,
                     CURLMsg *   const curlMsgP) {
/*----------------------------------------------------------------------------
   Get the next message from the queue of things the Curl multi manager
   wants to say to us.

   Return the message as *curlMsgP.

   Iff there are no messages in the queue, return *endOfMessagesP == true.
-----------------------------------------------------------------------------*/
    int remainingMsgCount;
    CURLMsg * privateCurlMsgP;
        /* Note that this is a pointer into the multi manager's memory,
           so we have to use it under lock.
        */

    curlMultiP->lockP->acquire(curlMultiP->lockP);

    privateCurlMsgP = curl_multi_info_read(curlMultiP->curlMultiP,
                                           &remainingMsgCount);

    if (privateCurlMsgP == NULL)
        *endOfMessagesP = true;
    else {
        *endOfMessagesP = false;
        *curlMsgP = *privateCurlMsgP;
    }
    curlMultiP->lockP->release(curlMultiP->lockP);
}

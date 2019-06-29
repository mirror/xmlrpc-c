/* Copyright information is at end of file */

#include "xmlrpc_config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <xmlparse.h> /* Expat */

#include "bool.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"

#include "xmlparser.h"

struct _xml_element {
/*----------------------------------------------------------------------------
   Information about an XML element
-----------------------------------------------------------------------------*/
    struct _xml_element * parentP;
    const char * name;
    xmlrpc_mem_block * cdataP;    /* char */
    xmlrpc_mem_block * childrenP; /* xml_element* */
};

/* Check that we're using expat in UTF-8 mode, not wchar_t mode.
** If you need to use expat in wchar_t mode, write a subroutine to
** copy a wchar_t string to a char string & return an error for
** any non-ASCII characters. Then call this subroutine on all
** XML_Char strings passed to our event handlers before using the
** data. */
/* #if sizeof(char) != sizeof(XML_Char)
** #error expat must define XML_Char to be a regular char. 
** #endif
*/

#define XMLRPC_ASSERT_ELEM_OK(elem) \
    XMLRPC_ASSERT((elem) != NULL && (elem)->name != XMLRPC_BAD_POINTER)


void
xml_init(xmlrpc_env * const envP) {

    XMLRPC_ASSERT_ENV_OK(envP);
}



void
xml_term(void) {

}



static xml_element *
xmlElementNew(xmlrpc_env * const envP,
              const char * const name) {
/*----------------------------------------------------------------------------
   A new skeleton element object - ready to be filled in to represent an
   actual element.
-----------------------------------------------------------------------------*/
    xml_element * retval;
    int name_valid, cdata_valid, children_valid;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(name != NULL);

    /* Set up our error-handling preconditions. */
    retval = NULL;
    name_valid = cdata_valid = children_valid = 0;

    /* Allocate our xml_element structure. */
    retval = (xml_element*) malloc(sizeof(xml_element));
    XMLRPC_FAIL_IF_NULL(retval, envP, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for XML element");

    /* Set our parent field to NULL. */
    retval->parentP = NULL;

    /* Copy over the element name. */
    retval->name = xmlrpc_strdupnull(name);
    XMLRPC_FAIL_IF_NULL(retval->name, envP, XMLRPC_INTERNAL_ERROR,
                        "Couldn't allocate memory for XML element");
    name_valid = 1;

    retval->cdataP = XMLRPC_MEMBLOCK_NEW(char, envP, 0);
    XMLRPC_FAIL_IF_FAULT(envP);
    cdata_valid = 1;

    retval->childrenP = XMLRPC_MEMBLOCK_NEW(xml_element *, envP, 0);
    XMLRPC_FAIL_IF_FAULT(envP);
    children_valid = 1;

 cleanup:
    if (envP->fault_occurred) {
        if (retval) {
            if (name_valid)
                xmlrpc_strfree(retval->name);
            if (cdata_valid)
                XMLRPC_MEMBLOCK_FREE(char, retval->cdataP);
            if (children_valid)
                XMLRPC_MEMBLOCK_FREE(xml_element *, retval->childrenP);
            free(retval);
        }
        return NULL;
    } else {
        return retval;
    }
}


/*=========================================================================
**  xml_element_free
**=========================================================================
**  Blow away an existing element & all of its child elements.
*/
void
xml_element_free(xml_element * const elemP) {

    xmlrpc_mem_block * childrenP;
    size_t size, i;
    xml_element ** contents;

    XMLRPC_ASSERT_ELEM_OK(elemP);

    xmlrpc_strfree(elemP->name);
    elemP->name = XMLRPC_BAD_POINTER;

    XMLRPC_MEMBLOCK_FREE(char, elemP->cdataP);

    /* Deallocate all of our children recursively. */
    childrenP = elemP->childrenP;
    contents = XMLRPC_MEMBLOCK_CONTENTS(xml_element *, childrenP);
    size = XMLRPC_MEMBLOCK_SIZE(xml_element *, childrenP);
    for (i = 0; i < size; ++i)
        xml_element_free(contents[i]);

    XMLRPC_MEMBLOCK_FREE(xml_element *, elemP->childrenP);

    free(elemP);
}


/*=========================================================================
**  Miscellaneous Accessors
**=========================================================================
**  Return the fields of the xml_element. See the header for more
**  documentation on each function works.
*/



const char *
xml_element_name(const xml_element * const elemP) {

    XMLRPC_ASSERT_ELEM_OK(elemP);

    return elemP->name;
}



size_t
xml_element_cdata_size (const xml_element * const elemP) {
/*----------------------------------------------------------------------------
  The result of this function is NOT VALID until the end_element handler
  has been called!
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ELEM_OK(elemP);

    return XMLRPC_MEMBLOCK_SIZE(char, elemP->cdataP) - 1;
}



const char *
xml_element_cdata(const xml_element * const elemP) {

    XMLRPC_ASSERT_ELEM_OK(elemP);

    return XMLRPC_TYPED_MEM_BLOCK_CONTENTS(const char, elemP->cdataP);
}



unsigned int
xml_element_children_size(const xml_element * const elemP) {

    XMLRPC_ASSERT_ELEM_OK(elemP);

    return XMLRPC_MEMBLOCK_SIZE(xml_element *, elemP->childrenP);
}



xml_element **
xml_element_children(const xml_element * const elemP) {
    XMLRPC_ASSERT_ELEM_OK(elemP);
    return XMLRPC_MEMBLOCK_CONTENTS(xml_element *, elemP->childrenP);
}



/*=============================================================================
  Internal xml_element Utility Functions
=============================================================================*/

static void
xml_element_append_cdata(xmlrpc_env *  const envP,
                         xml_element * const elemP,
                         const char *  const cdata,
                         size_t        const size) {

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_ELEM_OK(elemP);

    XMLRPC_MEMBLOCK_APPEND(char, envP, elemP->cdataP, cdata, size);
}



static void
xml_element_append_child(xmlrpc_env *  const envP,
                         xml_element * const elemP,
                         xml_element * const childP) {
/*----------------------------------------------------------------------------
  Whether or not this function succeeds, it takes ownership of *childP.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT_ELEM_OK(elemP);
    XMLRPC_ASSERT_ELEM_OK(childP);
    XMLRPC_ASSERT(childP->parentP == NULL);

    XMLRPC_MEMBLOCK_APPEND(xml_element *, envP, elemP->childrenP, &childP, 1);
    if (!envP->fault_occurred)
        childP->parentP = elemP;
    else
        xml_element_free(childP);
}



typedef struct {
/*----------------------------------------------------------------------------
   Our parse context. We pass this around as expat user data.
-----------------------------------------------------------------------------*/
    xmlrpc_env        env;
    xml_element *     rootP;
    xml_element *     currentP;
    xmlrpc_mem_pool * memPoolP;
        /* The memory pool we use for as much memory allocation as we can;
           It's purpose is that it is of limited size, so especially use it
           for things that need to be limited, like to prevent an XML-RPC
           client from using up all the memory by sending a cleverly crafted
           XML document.
        */
} ParseContext;



static void
initParseContext(ParseContext *    const contextP,
                 xmlrpc_mem_pool * const memPoolP) {

    xmlrpc_env_init(&contextP->env);

    contextP->rootP    = NULL;
    contextP->currentP = NULL;
    contextP->memPoolP = memPoolP;
}



static void
termParseContext(ParseContext * const contextP) {

    xmlrpc_env_clean(&contextP->env);
}


/*=============================================================================
  Expat Event Handler Functions
=============================================================================*/

static void
startElement(void *      const userData,
             XML_Char *  const name,
             XML_Char ** const atts ATTR_UNUSED) {

    ParseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(name != NULL);

    if (!contextP->env.fault_occurred) {
        xml_element * elemP;

        elemP = xmlElementNew(&contextP->env, name);
        if (!contextP->env.fault_occurred) {
            XMLRPC_ASSERT(elemP != NULL);

            /* Insert the new element in the appropriate place. */
            if (!contextP->rootP) {
                /* No root yet, so this element must be the root. */
                contextP->rootP = elemP;
                contextP->currentP = elemP;
            } else {
                XMLRPC_ASSERT(contextP->currentP != NULL);

                /* (We need to watch our error handling invariants
                   very carefully here. Read the docs for
                   xml_element_append_child.
                */
                xml_element_append_child(&contextP->env, contextP->currentP,
                                         elemP);
                if (!contextP->env.fault_occurred)
                    contextP->currentP = elemP;
            }
            if (contextP->env.fault_occurred)
                xml_element_free(elemP);
        }
        if (contextP->env.fault_occurred) {
            /* Having changed *contextP to reflect failure, we are responsible
               for undoing everything that has been done so far in this
               context.
            */
            if (contextP->rootP)
                xml_element_free(contextP->rootP);
        }
    }
}



static void
endElement(void *     const userData,
           XML_Char * const name ATTR_UNUSED) {

    ParseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(name != NULL);

    if (!contextP->env.fault_occurred) {
        /* I think Expat enforces these facts: */
        XMLRPC_ASSERT(xmlrpc_streq(name, contextP->currentP->name));
        XMLRPC_ASSERT(contextP->currentP->parentP != NULL ||
                      contextP->currentP == contextP->rootP);

        /* Add a trailing NUL to our cdata. */
        xml_element_append_cdata(&contextP->env, contextP->currentP, "\0", 1);
        if (!contextP->env.fault_occurred)
            /* Pop our "stack" of elements. */
            contextP->currentP = contextP->currentP->parentP;

        if (contextP->env.fault_occurred) {
            /* Having changed *contextP to reflect failure, we are responsible
               for undoing everything that has been done so far in this
               context.
            */
            if (contextP->rootP)
                xml_element_free(contextP->rootP);
        }
    }
}



static void
characterData(void *     const userData,
              XML_Char * const s,
              int        const len) {
/*----------------------------------------------------------------------------
   This is an Expat character data (cdata) handler.  When an Expat
   parser comes across cdata, he calls one of these with the cdata as
   argument.  He can call it multiple times for consecutive cdata.

   We simply append the cdata to the cdata buffer for whatever XML
   element the parser is presently parsing.
-----------------------------------------------------------------------------*/
    ParseContext * const contextP = userData;

    XMLRPC_ASSERT(contextP != NULL);
    XMLRPC_ASSERT(s != NULL);
    XMLRPC_ASSERT(len >= 0);

    if (!contextP->env.fault_occurred) {
        XMLRPC_ASSERT(contextP->currentP != NULL);
    
        xml_element_append_cdata(&contextP->env, contextP->currentP, s, len);
    }
}



static void
createParser(xmlrpc_env *      const envP,
             xmlrpc_mem_pool * const memPoolP,
             ParseContext *    const contextP,
             XML_Parser *      const parserP) {
/*----------------------------------------------------------------------------
   Create an Expat parser to parse our XML.

   Return the parser handle as *parserP.

   Set up *contextP as a context specific to this module for Expat to
   associate with the parser.  The XML element handlers in this module get a
   pointer to *contextP to use for context.

   Use memory pool *memPoolP for certain memory allocations needed to parse
   the document.  Especially allocations for which we cannot predict a bound
   until we see the XML.
-----------------------------------------------------------------------------*/
    XML_Parser parser;

    parser = xmlrpc_XML_ParserCreate(NULL);
    if (parser == NULL)
        xmlrpc_faultf(envP, "Could not create expat parser");
    else {
        initParseContext(contextP, memPoolP);

        xmlrpc_XML_SetUserData(parser, contextP);
        xmlrpc_XML_SetElementHandler(
            parser,
            (XML_StartElementHandler) startElement,
            (XML_EndElementHandler) endElement);
        xmlrpc_XML_SetCharacterDataHandler(
            parser,
            (XML_CharacterDataHandler) characterData);
    }
    *parserP = parser;
}



static void
destroyParser(XML_Parser     const parser,
              ParseContext * const contextP) {

    termParseContext(contextP);

    xmlrpc_XML_ParserFree(parser);
}



void
xml_parse(xmlrpc_env *      const envP,
          const char *      const xmlData,
          size_t            const xmlDataLen,
          xmlrpc_mem_pool * const memPoolP,
          xml_element **    const resultPP) {
/*----------------------------------------------------------------------------
  This is an implementation of the interface declared in xmlparser.h.  This
  implementation uses Xmlrpc-c's private fork of Expat.
-----------------------------------------------------------------------------*/
    /* 
       This is an Expat driver.
   
       We set up event-based parser handlers for Expat and set Expat loose
       on the XML.  Expat walks through the XML, calling our handlers along
       the way.  Our handlers build up the element description in our
       'context' variable, so that when Expat is finished, our results are
       in 'context' and we just have to pluck them out.

       We should allow the user to specify the encoding in 'xmlData', but
       we don't.
    */
    XML_Parser parser;
    ParseContext context;

    XMLRPC_ASSERT_ENV_OK(envP);
    XMLRPC_ASSERT(xmlData != NULL);

    createParser(envP, memPoolP, &context, &parser);

    if (!envP->fault_occurred) {
        bool ok;

        ok = xmlrpc_XML_Parse(parser, xmlData, xmlDataLen, 1);
            /* sets 'context', *envP */
        if (!ok) {
            /* Expat failed on its own to parse it -- this is not an error
               that our handlers detected.
            */
            xmlrpc_env_set_fault(
                envP, XMLRPC_PARSE_ERROR,
                xmlrpc_XML_GetErrorString(parser));
            if (!context.env.fault_occurred) {
                /* Have to clean up what our handlers built before Expat
                   barfed.
                */
                if (context.rootP)
                    xml_element_free(context.rootP);
            }
        } else {
            /* Expat got through the XML OK, but when it called our handlers,
               they might have detected a problem.  They would have noted
               such a problem in *contextP.
            */
            if (context.env.fault_occurred)
                xmlrpc_env_set_fault_formatted(
                    envP, context.env.fault_code,
                    "XML doesn't parse.  %s", context.env.fault_string);
            else {
                XMLRPC_ASSERT(context.rootP != NULL);
                XMLRPC_ASSERT(context.currentP == NULL);
                
                *resultPP = context.rootP;
            }
        }
        destroyParser(parser, &context);
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
** SUCH DAMAGE. */

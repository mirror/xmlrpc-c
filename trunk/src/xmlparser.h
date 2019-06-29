/* Copyright and license information is at the end of the file */

#ifndef XMLRPC_XMLPARSER_H_INCLUDED
#define XMLRPC_XMLPARSER_H_INCLUDED

#include "xmlrpc-c/util_int.h"
/*=============================================================================
  Abstract XML Parser Interface
===============================================================================
  This file provides an abstract interface to the XML parser, so we can
  use multiple XML parsers with the main XML-RPC code not having to know
  their specific interfaces.

  It works like this: When you need to parse some XML (we don't care what the
  XML is as long as it is one valid XML element, but in practice it is going
  to be an XML-RPC call or response), you call our 'xml_init' with the XML
  text as argument.  That generates an object of type 'xml_element' to
  represent the XML element.

  You then call methods of that object to parse the XML element, e.g. to
  find out its name or its children.
=============================================================================*/


typedef struct _xml_element xml_element;
    /* An object that represents an XML element */

void
xml_element_free(xml_element * const elemP);
    /* Destroy the element */

const char *
xml_element_name(const xml_element * const elemP);
    /* The XML element name.  UTF-8.
       
       This points to memory owned by *elemP.
    */

size_t
xml_element_cdata_size(const xml_element * const elemP);
    /* The size of the element's cdata */

const char *
xml_element_cdata(const xml_element * const elemP);
    /* The XML element's CDATA.  UTF-8.

       This is a pointer to the cdata.
       
       There is a NUL character appended to the CDATA, but it is not part of
       the CDATA, so is not included in the value returned by
       xml_element_cdata_size().
  
       This is memory owned by *elemP.
  
       The implementation is allowed to concatenate all the CDATA in the
       element regardless of child elements. Alternatively, if there are
       any child elements, the implementation is allowed to dispose
       of whitespace characters.
    */

unsigned int
xml_element_children_size(const xml_element * const elemP);
    /* Number of children the XML element has */

xml_element **
xml_element_children(const xml_element * const elemP);
    /* All the child elements of the element.

       Each pointer points to an object owned by *elemP.
    */

void
xml_parse(xmlrpc_env *      const envP,
          const char *      const xmlData,
          size_t            const xmlDataLen,
          xmlrpc_mem_pool * const memPoolP,
          xml_element **    const resultPP);
    /* 
       Create an xml_elemnt object to represent it.

       Caller must ultimately destroy this object.

       Parse the XML text 'xmlData', of length 'xmlDataLen'.  Return the
       description of the element that the XML text contains as *resultPP.
       Normally, the element has children, so that *resultPP is just the
       root of a tree of elements.

       Use *memPoolP for some memory allocations.  It is primarily for memory
       uses whose size we cannot bound right now, like because it depends on
       what the XML looks like - *memPoolP has bounds, so this prevents us
       from using more than our share of system memory.  If 'memPoolP' is
       null, just use the default system pool (which is unbounded).
    */


/* Initialize and terminate static global parser state.  This should be done
   once per run of a program, and while the program is just one thread.
*/
void
xml_init(xmlrpc_env * const envP);

void
xml_term(void);


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

#endif

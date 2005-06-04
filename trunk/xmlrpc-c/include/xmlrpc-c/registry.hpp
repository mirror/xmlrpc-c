#ifndef REGISTRY_HPP_INCLUDED
#define REGISTRY_HPP_INCLUDED

#include <string>
#include <vector>
#include <list>

#include <xmlrpc-c/server.h>
#include <xmlrpc-c/base.hpp>

namespace xmlrpc_c {


class method {
/*----------------------------------------------------------------------------
   An XML-RPC method.

   This base class is abstract.  You can't create an object in it.
   Define a useful method with this as a base class, with an
   execute() method.
-----------------------------------------------------------------------------*/
public:
    method();

    virtual ~method();

    virtual void
    execute(xmlrpc_c::paramList const& paramList,
            xmlrpc_c::value *   const  resultP) = 0;

    std::string signature() const { return _signature; };
    std::string help() const { return _help; };

    void
    incref();

    void
    decref(bool * const unreferencedP);

    // self() is a strange concession to the fact that we interface with
    // C code.  C code needs a regular pointer to this method, but our
    // C++ interface carefully prevents one from making such a pointer,
    // since it would be an uncounted reference.  So users of self() must
    // make sure that the reference it returns is always subordinate to a
    // methodPtr reference.

    xmlrpc_c::method * self();

protected:
    std::string _signature;
    std::string _help;

private:
    unsigned int refcount;
};

/* Example of a specific method class:

   class sample_add : public method {
   public:
       sample_add() {
           this->_signature = "ii";
           this->_help = "This method adds two integers together";
       }
       void
       execute(xmlrpc_c::param_list    const paramList,
               const xmlrpc_c::value * const retvalP) {
          
           int const addend(paramList.getInt(0));
           int const adder(paramList.getInt(1));

           *retvalP = xmlrpc_c::value(addend, adder);
      }
   };


   Example of creating such a method:

   methodPtr const sampleAddMethodP(new sample_add);

   You pass around, copy, etc. the handle sampleAddMethodP and when
   the last copy of the handle is gone, the sample_add object itself
   gets deleted.

*/


class methodPtr {
/*----------------------------------------------------------------------------
   This is a handle for a xmlrpc_c::method.  It maintains the reference
   count in the xmlrpc_c::method and destroys it when the count goes to
   zero, so the xmlrpc_c::method object lives until the last handle to it
   goes away.
-----------------------------------------------------------------------------*/
public:
    methodPtr();
    methodPtr(xmlrpc_c::method * const _methodP);

    methodPtr(xmlrpc_c::methodPtr const& methodPtr);
    
    ~methodPtr();
    
    methodPtr
    operator=(xmlrpc_c::methodPtr const& methodPtr);

    xmlrpc_c::method *
    operator->() const;

    xmlrpc_c::method *
    get() const;
        // Ordinarily, this would not be exposed, but we need this in
        // order to interface with the C registry code, which registers
        // a C pointer.  In a pure C++ implementation, we'd just
        // register a copy of the methodPtr object and our 'methodP'
        // member would be totally private.

private:
    xmlrpc_c::method * methodP;
};



class registry {
/*----------------------------------------------------------------------------
   An Xmlrpc-c server method registry.  An Xmlrpc-c server transport
   (e.g.  an HTTP server) uses this object to process an incoming
   Xmlrpc-c call.
-----------------------------------------------------------------------------*/

public:

    registry();
    ~registry();

    void
    registry::addMethod(string              const name,
                        xmlrpc_c::methodPtr const methodP);
    
    void
    registry::processCall(std::string    const& body,
                          std::string ** const  responsePP) const;

    xmlrpc_registry *
    registry::c_registry() const;
        /* This is meant to be private except to other objects in the
           Xmlrpc-c library.
        */

private:

    xmlrpc_registry * c_registryP;
        /* Pointer to the C registry object we use to implement this
           object.
        */

    list<xmlrpc_c::methodPtr> methodList;
        /* This is a list of all the method objects (actually, pointers
           to them).  But since the real registry is the C registry object,
           all this list is for is to maintain references to the objects
           to which the C registry points so that they continue to exist.
        */
};
} // namespace

#endif

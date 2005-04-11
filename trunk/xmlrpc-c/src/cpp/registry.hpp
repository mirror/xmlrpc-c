#ifndef REGISTRY_HPP_INCLUDED
#define REGISTRY_HPP_INCLUDED

#include <vector>

#include "xmlrpc.hpp"

namespace xmlrpc_c {


class method {
/*----------------------------------------------------------------------------
   An XML-RPC method.

   This base class is useless by itself.  Define a useful method with this
   as a base class.
-----------------------------------------------------------------------------*/
public:
    method() : 
        _signature("?"),
        _help("No help is available for this method") 
        {};

    virtual ~method();

    virtual void
    execute(std::vector<xmlrpc_c::value> const params,
            xmlrpc_c::value **           const resultP);

    string signature() const { return _signature; };
    string help() const { return _help; };

protected:
    string _signature;
    string _help;
};

/* Example of a specific method class:

   class sample_add : public method {
   public:
       sample_add() {
           this->_signature = "FILL IN SIGNATURE HERE";
           this->_help = "This method adds two integers together";
       }
       virtual void
       execute(std::vector<xmlrpc_c::value> const params,
               xmlrpc_c::value *            const retvalP) {
          
           *retvalP = xmlrpc_c::value((int)params[0] + (int)params[1]);
      }
   };
*/



class registry {
/*----------------------------------------------------------------------------
   An XMLRPC-C server method registry.  An XMLRPC-C server transport
   (e.g.  an HTTP server) uses this object to process an incoming
   XMLRPC-C call.
-----------------------------------------------------------------------------*/

public:

    registry();
    ~registry();

    void
    registry::addMethod(string           const name,
                        xmlrpc_c::method const method);


    void
    registry::processCall(string    const& body,
                          string ** const  responsePP) const;

private:

    xmlrpc_registry * c_registryP;
        /* Pointer to the C registry object we use to implement this
           object.
        */
};

} // namespace

#endif

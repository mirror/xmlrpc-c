#include <cassert>

#include <xmlrpc.hpp>
#include <registry.hpp>
#include <server_abyss.hpp>

using namespace std;

class sampleAddMethod : public xmlrpc_c::method {
public:
    sampleAddMethod() {
        // signature and help strings are documentation -- the client
        // can query this information with a system.methodSignature and
        // system.methodHelp RPC.
        this->_signature = "ii";  // method's arguments are two integers
        this->_help = "This method adds two integers together";
    }
    void
    execute(vector<xmlrpc_c::value>  const params,
            const xmlrpc_c::value ** const retvalP) {

        xmlrpc_c::value_int addend(params[0]);
        xmlrpc_c::value_int adder(params[1]);

        *retvalP = new xmlrpc_c::value_int((int)addend + (int)adder);
    }
};



int 
main(int           const argc, 
     const char ** const argv) {

    if (argc && argv) {  // defeat unused parameter warning
    };
    xmlrpc_c::registry myRegistry;

    xmlrpc_c::method_ptr const sampleAddMethodP(new sampleAddMethod);

    myRegistry.addMethod("sample.add", sampleAddMethodP);

    xmlrpc_c::server_abyss myAbyssServer(
        myRegistry,
        8080,              // TCP port on which to listen
        "/tmp/xmlrpc_log"  // Log file
        );

    myAbyssServer.run();
    // xmlrpc_c::server_abyss.run() never returns
    assert(false);

    return 0;
}

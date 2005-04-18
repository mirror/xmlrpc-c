#include <string>

#include "xmlrpc-c/base.hpp"

using namespace std;

namespace xmlrpc_c {
    
fault::fault(string                  const _description,
             xmlrpc_c::fault::code_t const _code
                 = xmlrpc_c::fault::CODE_UNSPECIFIED) :
    code(_code),
    description(_description)
    {}

xmlrpc_c::fault::code_t
fault::getCode() const {
    return this->code;
}

string
fault::getDescription() const {
    return this->description;
}

} // namespace

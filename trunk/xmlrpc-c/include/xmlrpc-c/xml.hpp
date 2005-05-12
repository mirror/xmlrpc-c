#ifndef XML_HPP_INCLUDED
#define XML_HPP_INCLUDED

#include <string>
#include <xmlrpc-c/base.hpp>

namespace xmlrpc_c {
namespace xml {

void
generateCall(std::string         const& methodName,
             xmlrpc_c::paramList const& paramList,
             std::string *       const  callXmlP);
    
void
parseResponse(std::string       const& responseXml,
              xmlrpc_c::value * const  resultP);

void
trace(std::string const& label,
      std::string const& xml);


}} // namespace
#endif

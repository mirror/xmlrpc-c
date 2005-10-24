/*=============================================================================
                                curl.cpp
===============================================================================
  This is the Curl XML transport of the C++ XML-RPC client library for
  Xmlrpc-c.

  Note that unlike most of Xmlprc-c's C++ API, this is _not_ based on the
  C client library.  This code is independent of the C client library, and
  is based directly on the client XML transport libraries (with a little
  help from internal C utility libraries).
=============================================================================*/

#include <stdlib.h>
#include <cassert>
#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"
#include "xmlrpc-c/base_int.h"

#include "xmlrpc_curl_transport.h"

/* transport_config.h defines MUST_BUILD_CURL_CLIENT */
#include "transport_config.h"

#include "xmlrpc-c/client.hpp"


using namespace std;
using namespace xmlrpc_c;


namespace {

unsigned int
uintFromString(string const arg) {
    unsigned long retval;
    
    if (arg.length() == 0)
        throwf("Null string");
    
    char * tail;
    retval = strtoul(arg.c_str(), &tail, 10);
    if (*tail)
            throwf("String contains non-numeric stuff: '%s'", tail);
    return retval;
}



class optionSetting {
public:
    string optionName;
    bool   hasValue;
    string optionValue;

    optionSetting(string const optionName) :
        optionName(optionName), hasValue(false) {}

    optionSetting(string const optionName,
                  string const optionValue) :
        optionName(optionName), hasValue(true), optionValue(optionValue) {}

    void
    verifyHasValue() const {
        if (!this->hasValue)
            throwf("%s option must have value", this->optionName.c_str());
    }

    void
    verifyHasNoValue() const {
        if (this->hasValue)
            throwf("%s option must not have value", this->optionName.c_str());
    }

    const char *
    optionValueCstr() const {
        const char * retval;
        retval = strdup(this->optionValue.c_str());
        if (retval == NULL)
            throwf("Failed to allocate memory for '%s' option value",
                   this->optionName.c_str());
        return retval;
    }
};



class optionIterator {
/* An object of this class iterates through an option string that looks like
   "capath=/etc/ssl/ca no_ssl_verifyhost user_agent='my program'"
   and returns in each iteration
   information about one of those options -- option name and value if any
   A single space separates options -- 2 spaces in a row means a null
   option!  Single quotes anywhere hide blanks; they don't delimit tokens.
   E.g. "user_'agent'='my ''program'" works.  A doubled single quote within
   quotes stands for a single single quote.
*/
public:
    optionIterator(string const optionString)
        : optionString(optionString), cursor(0) {}

    bool
    endOfString() {
        return cursor >= optionString.length();
    }

    optionSetting
    next();
            
private:
    string const optionString;
    size_t cursor;

    enum tokenType {BLANK, EQUAL, STRING};

    struct token {
        enum tokenType type;
        string value;
    };

    token
    nextToken();

    void
    doRestOfNameEqualValue(string const&, optionSetting ** const);

    token
    nextQuotedString();

    token
    nextUnquotedString();
};



optionIterator::token
optionIterator::nextQuotedString() {
/*----------------------------------------------------------------------------
   Move past a quoted string and return its contents.  A quoted string
   begins with and ends with a single quote.

   A doubled single quote within a quoted string represents a single
   single quote.
-----------------------------------------------------------------------------*/
    assert(optionString[cursor] == '\'');

    token retval;

    retval.type = STRING;
    
    ++this->cursor;  // move past open quote

    bool closeQuoteFound;
    
    closeQuoteFound = false;
    while (!closeQuoteFound) {
        if (this->cursor >= optionString.length())
            throw(error("unterminated quoted string"));
        if (optionString[this->cursor] == '\'') {
            // Here's a quote.  It could be either the close quote we're
            // looking for or a doubled quote.
            if (this->cursor + 1 < optionString.length() &&
                optionString[this->cursor+1] == '\'') {
                this->cursor += 2;  // move past doubled quote
                retval.value += "'";  // output a single quote
            } else {
                closeQuoteFound = true;
                ++this->cursor;  // move past close quote
            }
        } else {
            // Just an ordinary char in the quoted string
            retval.value += optionString[this->cursor];
            ++this->cursor;
        }
    }
    return retval;
}




optionIterator::token
optionIterator::nextUnquotedString() {
/*----------------------------------------------------------------------------
   Move past and return an unquoted string.  An unquoted string starts
   with a character other than a delimiter character (blank, quote, or equal)
   and ends with a delimiter character or end of string.

   We return the largest unquoted string we can.
-----------------------------------------------------------------------------*/
    token retval;

    retval.type = STRING;
    
    while (this->cursor < optionString.length() &&
           optionString[cursor] != ' ' &&
           optionString[cursor] != '=' &&
           optionString[cursor] != '\'') {
        retval.value += optionString[this->cursor];
        ++this->cursor;
    }
    // Cursor now points to the delimiter character or end of string

    return retval;
}



optionIterator::token
optionIterator::nextToken() {
/*----------------------------------------------------------------------------
   Return the token at the current cursor position in the option string.
   A token is one of these:

   - a blank delimiter

   - an equal sign delimiter

   - a quoted string (value doesn't include the quotes)
 
   - an unquoted string: maximum consecutive characters not including blank,
     equal, or quote.
-----------------------------------------------------------------------------*/
    token retval;

    if (optionString[this->cursor] == ' ') {
        ++this->cursor;  // move past blank
        retval.type = BLANK;
        retval.value = " ";
    } else if (optionString[this->cursor] == '=') {
        ++this->cursor;  // move past equal sign
        retval.type = EQUAL;
        retval.value = "=";
    } else if (optionString[this->cursor] == '\'')
        retval = nextQuotedString();
    else
        retval = nextUnquotedString();

    return retval;
}



void
optionIterator::doRestOfNameEqualValue(string           const& optionName,
                                       optionSetting ** const  settingPP) {
/*----------------------------------------------------------------------------
   Parse the remainder of a name=value thing.  The iterator (*this) is
   presently positioned to the token after the = .
-----------------------------------------------------------------------------*/
    if (this->cursor >= optionString.length())
        throwf("No value after equal sign for option '%s'",
               optionName.c_str());
    else {
        token const optionValue(this->nextToken());
        if (optionValue.type != STRING)
            throwf("Invalid token '%s' after equal sign "
                   "for option '%s'",
                   optionValue.value.c_str(),
                   optionName.c_str());
        
        // Move past the delimiter after name=value
        if (cursor < optionString.length()) {
            token const delimiter(this->nextToken());

            if (delimiter.type != BLANK)
                throwf("Token following the '%s' option setting is "
                       "not a blank.  It is '%s'",
                       optionName.c_str(),
                       delimiter.value.c_str());
        }
        *settingPP = new optionSetting(optionName, optionValue.value);
    }
}



optionSetting
optionIterator::next() {

    optionSetting * settingP;

    if (cursor >= optionString.length())
        throw(error("End of string"));
    
    token const optionName(this->nextToken());
    
    if (optionName.type != STRING)
        throwf("Invalid token '%s' where option name expected.",
               optionName.value.c_str());
    
    if (cursor >= optionString.length())
        settingP = new optionSetting(optionName.value);
    else {
        token const delimiter(this->nextToken());
        if (delimiter.type == BLANK)
            settingP = new optionSetting(optionName.value);
        else if (delimiter.type == EQUAL)
            doRestOfNameEqualValue(optionName.value, &settingP);
        else
            throwf("Unexpected token '%s' where equal sign or blank "
                   "expected.", delimiter.value.c_str());
    }
    optionSetting const retval(*settingP);
    delete(settingP);

    return retval;
}



static void
processSetting(optionSetting                   const setting,
               struct xmlrpc_curl_xportparms * const transportParmsP) {

    /* Note that where the option names correlate with Curl options,
       we use the same name as Curl does (except lower case, and with
       CURLOPT_ removed).  That makes it easier for the user who, in
       spite of modularity ideals, will probably be familiar with the
       Curl options.
    */

    if (setting.optionName == "network_interface") {
        setting.verifyHasValue();
        transportParmsP->network_interface = setting.optionValueCstr();
    } else if (setting.optionName == "no_ssl_verifypeer") {
        setting.verifyHasNoValue();
        transportParmsP->no_ssl_verifypeer = true;
    } else if (setting.optionName == "no_ssl_verifyhost") {
        setting.verifyHasNoValue();
        transportParmsP->no_ssl_verifyhost = true;
    } else if (setting.optionName == "user_agent") {
        setting.verifyHasValue();
        transportParmsP->user_agent = setting.optionValueCstr();
    } else if (setting.optionName == "ssl_cert") {
        setting.verifyHasValue();
        transportParmsP->ssl_cert = setting.optionValueCstr();
    } else if (setting.optionName == "sslcerttype") {
        setting.verifyHasValue();
        transportParmsP->sslcerttype = setting.optionValueCstr();
    } else if (setting.optionName == "sslcertpasswd") {
        setting.verifyHasValue();
        transportParmsP->sslcertpasswd = setting.optionValueCstr();
    } else if (setting.optionName == "sslkey") {
        setting.verifyHasValue();
        transportParmsP->sslkey = setting.optionValueCstr();
    } else if (setting.optionName == "sslkeytype") {
        setting.verifyHasValue();
        transportParmsP->sslkeytype = setting.optionValueCstr();
    } else if (setting.optionName == "sslkeypasswd") {
        setting.verifyHasValue();
        transportParmsP->sslkeypasswd = setting.optionValueCstr();
    } else if (setting.optionName == "sslengine") {
        setting.verifyHasValue();
        transportParmsP->sslengine = setting.optionValueCstr();
    } else if (setting.optionName == "sslengine_default") {
        setting.verifyHasNoValue();
        transportParmsP->sslengine_default = true;
    } else if (setting.optionName == "sslversion") {
        setting.verifyHasValue();

        try {
            transportParmsP->sslversion = uintFromString(setting.optionValue);
        } catch (error errorDesc) {
            throwf("Invalid value '%s' for %s option.  %s",
                   setting.optionValue.c_str(), setting.optionName.c_str(),
                   errorDesc.what());
        }
    } else if (setting.optionName == "cainfo") {
        setting.verifyHasValue();
        transportParmsP->cainfo = setting.optionValueCstr();
    } else if (setting.optionName == "capath") {
        setting.verifyHasValue();
        transportParmsP->capath = setting.optionValueCstr();
    } else if (setting.optionName == "randomfile") {
        setting.verifyHasValue();
        transportParmsP->randomfile = setting.optionValueCstr();
    } else if (setting.optionName == "egdsocket") {
        setting.verifyHasValue();
        transportParmsP->egdsocket = setting.optionValueCstr();
    } else if (setting.optionName == "ssl_cipher_list") {
        setting.verifyHasValue();
        transportParmsP->ssl_cipher_list = setting.optionValueCstr();
    } else {
        throwf("Unrecognized option name: '%s'", setting.optionName.c_str());
    }
}



class xportParmsCWrapper {
public:
    xportParmsCWrapper() {
        this->transportParms.no_ssl_verifypeer = false;
        this->transportParms.no_ssl_verifyhost = false;
        this->transportParms.network_interface = NULL;
        this->transportParms.user_agent        = NULL;
        this->transportParms.ssl_cert          = NULL;
        this->transportParms.sslcerttype       = NULL;
        this->transportParms.sslcertpasswd     = NULL;
        this->transportParms.sslkey            = NULL;
        this->transportParms.sslkeytype        = NULL;
        this->transportParms.sslkeypasswd      = NULL;
        this->transportParms.sslengine         = NULL;
        this->transportParms.sslengine_default = false;
        this->transportParms.sslversion        = 0;
        this->transportParms.cainfo            = NULL;
        this->transportParms.capath            = NULL;
        this->transportParms.randomfile        = NULL;
        this->transportParms.egdsocket         = NULL;
        this->transportParms.ssl_cipher_list   = NULL;
        
    }

    ~xportParmsCWrapper() {
        if (this->transportParms.network_interface)
            xmlrpc_strfree(this->transportParms.network_interface);
        if (this->transportParms.user_agent)
            xmlrpc_strfree(this->transportParms.user_agent);
        if (this->transportParms.ssl_cert)
            xmlrpc_strfree(this->transportParms.ssl_cert);
        if (this->transportParms.sslcerttype)
            xmlrpc_strfree(this->transportParms.sslcerttype);
        if (this->transportParms.sslcertpasswd)
            xmlrpc_strfree(this->transportParms.sslcertpasswd);
        if (this->transportParms.sslkey)
            xmlrpc_strfree(this->transportParms.sslkey);
        if (this->transportParms.sslkeytype)
            xmlrpc_strfree(this->transportParms.sslkeytype);
        if (this->transportParms.sslkeypasswd)
            xmlrpc_strfree(this->transportParms.sslkeypasswd);
        if (this->transportParms.sslengine)
            xmlrpc_strfree(this->transportParms.sslengine);
        if (this->transportParms.cainfo)
            xmlrpc_strfree(this->transportParms.cainfo);
        if (this->transportParms.capath)
            xmlrpc_strfree(this->transportParms.capath);
        if (this->transportParms.randomfile)
            xmlrpc_strfree(this->transportParms.randomfile);
        if (this->transportParms.egdsocket)
            xmlrpc_strfree(this->transportParms.egdsocket);
        if (this->transportParms.ssl_cipher_list)
            xmlrpc_strfree(this->transportParms.ssl_cipher_list);
    }

    size_t
    size() {
        return XMLRPC_CXPSIZE(ssl_cipher_list);
    }

    struct xmlrpc_curl_xportparms transportParms; 
};



} // namespace



namespace xmlrpc_c {

carriageParm_curl0::carriageParm_curl0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}


#if MUST_BUILD_CURL_CLIENT

clientXmlTransport_curl::clientXmlTransport_curl(
    clientXmlTransport_curl::optFormat const optFormat,
    std::string                        const optionString) {

    xportParmsCWrapper cWrapper;
    optionIterator iterator(optionString);

    if (optFormat != OPTFORMAT_1)
        throw(error("Unrecognized option string format value"));

    while (!iterator.endOfString()) {
        optionSetting const setting(iterator.next());
        processSetting(setting, &cWrapper.transportParms);
    }

    this->c_transportOpsP = &xmlrpc_curl_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_curl_transport_ops.create(
        &env, 0, "", "", (xmlrpc_xportparms *)&cWrapper.transportParms,
        cWrapper.size(),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



clientXmlTransport_curl::clientXmlTransport_curl(
    string const networkInterface,
    bool   const noSslVerifyPeer,
    bool   const noSslVerifyHost,
    string const userAgent) {

    struct xmlrpc_curl_xportparms transportParms; 

    transportParms.network_interface = 
        networkInterface.size() > 0 ? networkInterface.c_str() : NULL;
    transportParms.no_ssl_verifypeer = noSslVerifyPeer;
    transportParms.no_ssl_verifyhost = noSslVerifyHost;
    transportParms.user_agent =
        userAgent.size() > 0 ? userAgent.c_str() : NULL;

    this->c_transportOpsP = &xmlrpc_curl_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_curl_transport_ops.create(
        &env, 0, "", "", (xmlrpc_xportparms *)&transportParms,
        XMLRPC_CXPSIZE(user_agent),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}
#else  // MUST_BUILD_CURL_CLIENT

clientXmlTransport_curl::clientXmlTransport_curl(
    enum optFormat, optionString) {

    throw(error("There is no Curl client XML transport in this XML-RPC client "
                "library"));
}

clientXmlTransport_curl::clientXmlTransport_curl(string, bool, bool, string) {

    throw(error("There is no Curl client XML transport in this XML-RPC client "
                "library"));
}

#endif
 


clientXmlTransport_curl::~clientXmlTransport_curl() {

    this->c_transportOpsP->destroy(this->c_transportP);
}


} // namespace

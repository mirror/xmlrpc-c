#ifndef XMLRPC_HPP_INCLUDED
#define XMLRPC_HPP_INCLUDED

#include <vector>
#include <map>
#include <string>
#include <time.h>

#include "xmlrpc.h"

namespace xmlrpc_c {


class value {

    // This is a handle for the malloc'ed xmlrpc_value C object.
    // Nobody should create a pointer to xmlrpc_c::value object; it's
    // supposed to be an automatic (i.e. stack) variable.

public:
    value(xmlrpc_value * valueP);

    value(xmlrpc_c::value const &value);

    ~value();

    enum type_t {
        TYPE_INT        = 0,
        TYPE_BOOLEAN    = 1,
        TYPE_DOUBLE     = 2,
        TYPE_DATETIME   = 3,
        TYPE_STRING     = 4,
        TYPE_BYTESTRING = 5,
        TYPE_ARRAY      = 6,
        TYPE_STRUCT     = 7,
        TYPE_C_PTR      = 8,
        TYPE_NIL        = 9,
        TYPE_DEAD       = 0xDEAD,
    };

    type_t type() const;

    // These really don't fit as public, but I couldn't find any other
    // way to let the xmlprc_c::value_array() and xmlrpc_c::value_struct
    // constructors work.

    void
    append_to_c_array(xmlrpc_value * const arrayP) const;

    void
    add_to_c_struct(xmlrpc_value * const structP,
                    string         const key) const;

protected:
    value() {};  // must be followed up with instantiate()

    void
    instantiate(xmlrpc_value * const valueP);

    xmlrpc_value *
    c_value() const;

private:
    xmlrpc_value * c_valueP;
};



class value_int : public value {
public:
    value_int(int const cvalue);
};



class value_double : public value {
public:
    value_double(double const cvalue);
};



class value_boolean : public value {
public:
    value_boolean(bool const cvalue);
};



class value_datetime : public value {
public:
    value_datetime(std::string const cvalue);
    value_datetime(time_t const cvalue);
    value_datetime(struct timeval const& cvalue);
    value_datetime(struct timespec const& cvalue);
};



class value_string : public value {
public:
    value_string(std::string const& cvalue);
};



class value_bytestring : public value {
public:
    value_bytestring(std::vector<unsigned char> const& cvalue);
};



class value_array : public value {
public:
    value_array(vector<xmlrpc_c::value> const& cvalue);
};



class value_struct : public value {
public:
    value_struct(std::map<std::string, xmlrpc_c::value> const& cvalue);
};



class value_nil : public value {
public:
    value_nil();
};



class fault {
/*----------------------------------------------------------------------------
   This is an XML-RPC fault.

   This object is not intended to be used to represent a fault in the
   execution of XML-RPC client/server software -- just a fault in an
   XML-RPC RPC as described by the XML-RPC spec.

   There is no way to represent "no fault" with this object.  The object is
   meaningful only in the context of some fault.
-----------------------------------------------------------------------------*/
public:
    int faultCode;
    std::string faultDescription;
};

} // namespace

#endif

#include <climits>
#include <cfloat>
#include <ctime>
#include <string>

#include "girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


param_list::param_list(unsigned int const paramCount = 0) {

    this->paramVector.reserve(paramCount);
}


 
void
param_list::add(xmlrpc_c::value const param) {

    this->paramVector.push_back(param);
}



unsigned int
param_list::size() const {
    return this->paramVector.size();
}



xmlrpc_c::value 
param_list::operator[](unsigned int const subscript) const {

    if (subscript >= this->paramVector.size())
        throw(girerr::error(
            "Subscript of xmlrpc_c::param_list out of bounds"));

    return this->paramVector[subscript];
}



int
param_list::getInt(unsigned int const paramNumber,
                   int          const minimum = INT_MIN,
                   int          const maximum = INT_MAX) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_INT)
        throw(fault("Parameter that is supposed to be integer is not", 
                    fault::CODE_TYPE));

    int const intvalue(static_cast<int>(
        value_int(this->paramVector[paramNumber])));

    if (intvalue < minimum)
        throw(fault("Integer parameter too low", fault::CODE_TYPE));

    if (intvalue > maximum)
        throw(fault("Integer parameter too high", fault::CODE_TYPE));

    return intvalue;
}



bool
param_list::getBoolean(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_BOOLEAN)
        throw(fault("Parameter that is supposed to be boolean is not", 
                    fault::CODE_TYPE));

    return static_cast<bool>(value_boolean(this->paramVector[paramNumber]));
}



double
param_list::getDouble(unsigned int const paramNumber,
                      double       const minimum = DBL_MIN,
                      double       const maximum = DBL_MAX) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_DOUBLE)
        throw(fault("Parameter that is supposed to be floating point number "
                    "is not", 
                    fault::CODE_TYPE));

    double const doublevalue(static_cast<double>(
        value_double(this->paramVector[paramNumber])));

    if (doublevalue < minimum)
        throw(fault("Floating point number parameter too low",
                    fault::CODE_TYPE));

    if (doublevalue > maximum)
        throw(fault("Floating point number parameter too high",
                    fault::CODE_TYPE));

    return doublevalue;
}



time_t
param_list::getDatetime_sec(
    unsigned int                const paramNumber,
    param_list::time_constraint const constraint 
        = param_list::TC_ANY) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_DATETIME)
        throw(fault("Parameter that is supposed to be a datetime is not", 
                    fault::CODE_TYPE));

    time_t const timeValue(static_cast<time_t>(value_datetime(*paramP)));
    time_t const now(time(NULL));

    switch (constraint) {
    case TC_ANY:
        /* He'll take anything; no problem */
        break;
    case TC_NO_FUTURE:
        if (timeValue > now)
            throw(fault("Datetime parameter that is not supposed to be in "
                        "the future is.", fault::CODE_TYPE));
        break;
    case TC_NO_PAST:
        if (timeValue < now)
            throw(fault("Datetime parameter that is not supposed to be in "
                        "the past is.", fault::CODE_TYPE));
        break;
    }

    return timeValue;
}



string
param_list::getString(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_STRING)
        throw(fault("Parameter that is supposed to be a string is not", 
                    fault::CODE_TYPE));

    return static_cast<string>(value_string(this->paramVector[paramNumber]));
}



std::vector<unsigned char>
param_list::getBytestring(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_BYTESTRING)
        throw(fault("Parameter that is supposed to be a byte string is not", 
                    fault::CODE_TYPE));

    return value_bytestring(*paramP).vector_uchar_value();
}


std::vector<xmlrpc_c::value>
param_list::getArray(unsigned int const paramNumber,
                     unsigned int const minSize = 0,
                     unsigned int const maxSize = UINT_MAX) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_ARRAY)
        throw(fault("Parameter that is supposed to be an array is not", 
                    fault::CODE_TYPE));

    xmlrpc_c::value_array const arrayValue(*paramP);
    
    if (arrayValue.size() < minSize)
        throw(fault("Array parameter has too few elements",
                    fault::CODE_TYPE));
    
    if (arrayValue.size() > maxSize)
        throw(fault("Array parameter has too many elements",
                    fault::CODE_TYPE));

    return value_array(*paramP).vector_value_value();
}



std::map<string, xmlrpc_c::value>
param_list::getStruct(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    const xmlrpc_c::value * const paramP(&this->paramVector[paramNumber]);

    if (paramP->type() != value::TYPE_STRUCT)
        throw(fault("Parameter that is supposed to be a structure is not", 
                    fault::CODE_TYPE));

    return static_cast<std::map<string, xmlrpc_c::value> >(
        value_struct(*paramP));
}



void
param_list::getNil(unsigned int const paramNumber) const {

    if (paramNumber >= this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));

    if (this->paramVector[paramNumber].type() != value::TYPE_NIL)
        throw(fault("Parameter that is supposed to be nil is not", 
                    fault::CODE_TYPE));
}



void
param_list::verifyEnd(unsigned int const paramNumber) const {

    if (paramNumber < this->paramVector.size())
        throw(fault("Too many parameters", fault::CODE_TYPE));
    if (paramNumber > this->paramVector.size())
        throw(fault("Not enough parameters", fault::CODE_TYPE));
}

}  // namespace

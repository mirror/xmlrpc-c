/*****************************************************************************
                                xmlrpc_value.cpp
******************************************************************************
  This module provides services for dealwith XML-RPC values.  Each type
  of XML-RPC value is a C++ class.  An object represents a particular
  XML-RPC value.

  Everything is based on the C services in libxmlrpc.

  We could make things more efficient by using the internal interfaces
  via xmlrpc_int.h.  We could make them even more efficient by dumping
  libxmlrpc altogether for some or all of these services.

  An xmlrpc_c::value object is really just a handle for a C xmlrpc_value
  object.  You're not supposed to make a pointer to an xmlrpc_c::value
  object, but rather copy the object around.

  Because the C xmlrpc_value object does reference counting, it
  disappears automatically when the last handle does.  To go pure C++,
  we'd have to have a C++ object for the value itself and a separate
  handle object, like Boost's shared_ptr<>.

  The C++ is designed so that the user never sees the C interface at
  all.  Unfortunately, the user can see it if he wants because some
  class members had to be declared public so that other components of
  the library could see them, but the user is not supposed to access
  those members.

*****************************************************************************/

#include <string>
#include <vector>
#include <time.h>

#include "girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/base.hpp"

using namespace std;

namespace {

class cDatetimeValueWrapper {
public:
    xmlrpc_value * valueP;
    
    cDatetimeValueWrapper(time_t const cppvalue) {
        xmlrpc_env env;
        xmlrpc_env_init(&env);
        
        this->valueP = xmlrpc_datetime_new_sec(&env, cppvalue);
        if (env.fault_occurred)
            throw(error(env.fault_string));
    }
    ~cDatetimeValueWrapper() {
        xmlrpc_DECREF(this->valueP);
    }
};


class cStringWrapper {
public:
    const char * str;
    size_t length;
    cStringWrapper(xmlrpc_value * valueP) {
        xmlrpc_env env;
        xmlrpc_env_init(&env);

        xmlrpc_read_string_lp(&env, valueP, &length, &str);
        if (env.fault_occurred)
            throw(error(env.fault_string));
    }
    ~cStringWrapper() {
        free((char*)str);
    }
};
    

} // namespace



namespace xmlrpc_c {

value::value() {
    this->c_valueP = NULL;
}



value::value(xmlrpc_value * const valueP) {  // default constructor

    this->instantiate(valueP);
}



value::value(xmlrpc_c::value const& value) {  // copy constructor
    this->c_valueP = value.c_value();
}



xmlrpc_c::value&
value::operator=(xmlrpc_c::value const& value) {

    if (this->c_valueP != NULL)
        throw(error("Assigning to already instantiated xmlrpc_c::value"));

    this->c_valueP = value.c_value();
    return *this;  // The result of the (a = b) expression
}



value::~value() {
    if (this->c_valueP) {
        xmlrpc_DECREF(this->c_valueP);
    }
}



void
value::instantiate(xmlrpc_value * const valueP) {

    xmlrpc_INCREF(valueP);
    this->c_valueP = valueP;
}



xmlrpc_value *
value::c_value() const {

    if (this->c_valueP) {
        xmlrpc_INCREF(this->c_valueP);  // For Caller
    }
    return this->c_valueP;
}



void
value::append_to_c_array(xmlrpc_value * const arrayP) const {
/*----------------------------------------------------------------------------
  Append this value to the C array 'arrayP'.
----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_array_append_item(&env, arrayP, this->c_valueP);
    
    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
value::add_to_c_struct(xmlrpc_value * const structP,
                       string         const key) const {
/*----------------------------------------------------------------------------
  Add this value to the C array 'arrayP' with key 'key'.
----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_struct_set_value_n(&env, structP,
                              key.c_str(), key.length(),
                              this->c_valueP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



value::type_t 
value::type() const {
    /* You'd think we could just cast from xmlrpc_type to
       value:type_t, but Gcc warns if we do that.  So we have to do this
       even messier union nonsense.
    */
    union {
        xmlrpc_type   x;
        value::type_t y;
    } u;

    u.x = xmlrpc_value_type(this->c_valueP);

    return u.y;
}



value_int::value_int(int const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(int const cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_int_new(&env, cppvalue);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_int::value_int(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_INT)
        throw(error("Not integer type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_int::operator int() const {

    int retval;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_read_int(&env, this->c_valueP, &retval);
    if (env.fault_occurred)
        throw(error(env.fault_string));

    return retval;
}



value_double::value_double(double const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(double const cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_double_new(&env, cppvalue);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    this->instantiate(cWrapper(cppvalue).valueP);
}



value_double::value_double(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_DOUBLE)
        throw(error("Not double type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_double::operator double() const {

    double retval;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_read_double(&env, this->c_valueP, &retval);
    if (env.fault_occurred)
        throw(error(env.fault_string));

    return retval;
}



value_boolean::value_boolean(bool const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(xmlrpc_bool const cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_bool_new(&env, cppvalue);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_boolean::operator bool() const {

    xmlrpc_bool retval;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_read_bool(&env, this->c_valueP, &retval);
    if (env.fault_occurred)
        throw(error(env.fault_string));

    return (bool)retval;
}



value_boolean::value_boolean(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_BOOLEAN)
        throw(error("Not boolean type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_datetime::value_datetime(string const cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(string const cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_datetime_new_str(&env, cppvalue.c_str());
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_datetime::value_datetime(time_t const cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_datetime::value_datetime(struct timeval const& cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue.tv_sec);

    this->instantiate(wrapper.valueP);
}



value_datetime::value_datetime(struct timespec const& cppvalue) {

    cDatetimeValueWrapper wrapper(cppvalue.tv_sec);

    this->instantiate(wrapper.valueP);
}



value_datetime::value_datetime(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_DATETIME)
        throw(error("Not datetime type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_datetime::operator time_t() const {

    time_t retval;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_read_datetime_sec(&env, this->c_valueP, &retval);
    if (env.fault_occurred)
        throw(error(env.fault_string));

    return retval;
}



value_string::value_string(string const& cppvalue) {
    
    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(string const cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_string_new(&env, cppvalue.c_str());
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



value_string::value_string(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_STRING)
        throw(error("Not string type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_string::operator string() const {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    cStringWrapper adapter(this->c_valueP);

    return string(adapter.str, adapter.length);
}



value_bytestring::value_bytestring(
    vector<unsigned char> const& cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper(vector<unsigned char> const& cppvalue) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = 
                xmlrpc_base64_new(&env, cppvalue.size(), &cppvalue[0]);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper(cppvalue);
    
    this->instantiate(wrapper.valueP);
}



vector<unsigned char>
value_bytestring::vector_uchar_value() const {

    class cWrapper {
    public:
        const unsigned char * contents;
        size_t length;

        cWrapper(xmlrpc_value * const valueP) {
            xmlrpc_env env;
            xmlrpc_env_init(&env);

            xmlrpc_read_base64(&env, valueP, &length, &contents);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            free((void*)contents);
        }
    };
    
    cWrapper wrapper(this->c_valueP);

    return vector<unsigned char>(&wrapper.contents[0], 
                                 &wrapper.contents[wrapper.length]);
}



size_t
value_bytestring::length() const {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    
    size_t length;
    xmlrpc_read_base64_size(&env, this->c_valueP, &length);
    if (env.fault_occurred)
        throw(error(env.fault_string));

    return length;
}



value_bytestring::value_bytestring(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_BYTESTRING)
        throw(error("Not byte string type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_array::value_array(vector<xmlrpc_c::value> const& cppvalue) {
    
    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_array_new(&env);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;
    
    vector<xmlrpc_c::value>::const_iterator i;
    for (i = cppvalue.begin(); i != cppvalue.end(); ++i)
        i->append_to_c_array(wrapper.valueP);

    this->instantiate(wrapper.valueP);
}



value_array::value_array(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_ARRAY)
        throw(error("Not array type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



vector<xmlrpc_c::value>
value_array::vector_value_value() const {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    unsigned int arraySize;

    arraySize = xmlrpc_array_size(&env, this->c_valueP);
    if (env.fault_occurred)
        throw(error(env.fault_string));
    
    vector<xmlrpc_c::value> retval(arraySize);
    
    for (unsigned int i = 0; i < arraySize; ++i) {

        class cWrapper {
        public:
            xmlrpc_value * valueP;

            cWrapper(xmlrpc_value * const arrayP,
                     unsigned int   const index) {

                xmlrpc_env env;
                xmlrpc_env_init(&env);

                xmlrpc_array_read_item(&env, arrayP, index, &valueP);
                
                if (env.fault_occurred)
                    throw(error(env.fault_string));
            }
            ~cWrapper() {
                xmlrpc_DECREF(valueP);
            }
        };

        cWrapper wrapper(this->c_valueP, i);

        retval[i].instantiate(wrapper.valueP);
    }

    return retval;
}



size_t
value_array::size() const {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    unsigned int arraySize;

    arraySize = xmlrpc_array_size(&env, this->c_valueP);
    if (env.fault_occurred)
        throw(error(env.fault_string));
    
    return arraySize;
}



value_struct::value_struct(
    map<string, xmlrpc_c::value> const &cppvalue) {

    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_struct_new(&env);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;

    map<string, xmlrpc_c::value>::const_iterator i;
    for (i = cppvalue.begin(); i != cppvalue.end(); ++i) {
        xmlrpc_c::value mapvalue(i->second);
        string          mapkey(i->first);
        mapvalue.add_to_c_struct(wrapper.valueP, mapkey);
    }
    
    this->instantiate(wrapper.valueP);
}



value_struct::value_struct(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_STRUCT)
        throw(error("Not struct type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



value_struct::operator map<string, xmlrpc_c::value>() const {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    unsigned int structSize;

    structSize = xmlrpc_struct_size(&env, this->c_valueP);
    if (env.fault_occurred)
        throw(error(env.fault_string));
    
    map<string, xmlrpc_c::value> retval;
    
    for (unsigned int i = 0; i < structSize; ++i) {
        class cMemberWrapper {
        public:
            xmlrpc_value * keyP;
            xmlrpc_value * valueP;

            cMemberWrapper(xmlrpc_value * const structP,
                           unsigned int   const index) {

                xmlrpc_env env;
                xmlrpc_env_init(&env);
            
                xmlrpc_struct_read_member(&env, structP, index, 
                                          &keyP, &valueP);
                
                if (env.fault_occurred)
                    throw(error(env.fault_string));
            }
            ~cMemberWrapper() {
                xmlrpc_DECREF(keyP);
                xmlrpc_DECREF(valueP);
            }
        };

        cMemberWrapper memberWrapper(this->c_valueP, i);

        cStringWrapper keyWrapper(memberWrapper.keyP);

        string const key(keyWrapper.str, keyWrapper.length);

        retval[key] = xmlrpc_c::value(memberWrapper.valueP);
    }

    return retval;
}



value_nil::value_nil() {
    
    class cWrapper {
    public:
        xmlrpc_value * valueP;
        
        cWrapper() {
            xmlrpc_env env;
            xmlrpc_env_init(&env);
            
            this->valueP = xmlrpc_nil_new(&env);
            if (env.fault_occurred)
                throw(error(env.fault_string));
        }
        ~cWrapper() {
            xmlrpc_DECREF(this->valueP);
        }
    };
    
    cWrapper wrapper;
    
    this->instantiate(wrapper.valueP);
}
    


value_nil::value_nil(xmlrpc_c::value const baseValue) {

    if (baseValue.type() != xmlrpc_c::value::TYPE_NIL)
        throw(error("Not nil type.  See type() method"));
    else {
        this->instantiate(baseValue.c_valueP);
    }
}



} // namespace


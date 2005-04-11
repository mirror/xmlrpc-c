#include <string>
#include <vector>
#include <time.h>

#include "girerr.hpp"
using girerr::throwf;
using girerr::error;
#include "xmlrpc.h"
#include "xmlrpc.hpp"


namespace {
 class cDatetimeWrapper {
 public:
     xmlrpc_value * valueP;
     
     cDatetimeWrapper(time_t const cppvalue) {
         xmlrpc_env env;
         xmlrpc_env_init(&env);
         
         this->valueP = xmlrpc_datetime_new_sec(&env, cppvalue);
         if (env.fault_occurred)
             throw(error(env.fault_string));
     }
     ~cDatetimeWrapper() {
         xmlrpc_DECREF(this->valueP);
     }
 };
} // namespace



namespace xmlrpc_c {

 value::value(xmlrpc_value * const valueP) {
     this->instantiate(valueP);
 }


 
 value::value(xmlrpc_c::value const& value) {
     this->c_valueP = value.c_value();
 }


 
 value::~value() {
     xmlrpc_DECREF(this->c_valueP);
 }



 void
 value::instantiate(xmlrpc_value * const valueP) {
     xmlrpc_INCREF(valueP);
     this->c_valueP = valueP;
 }

 

 xmlrpc_value *
 value::c_value() const {
     xmlrpc_INCREF(this->c_valueP);  // For Caller
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
     
     cWrapper wrapper(cppvalue);
     
     this->instantiate(wrapper.valueP);
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

 value_datetime::value_datetime(std::string const cppvalue) {

     class cWrapper {
     public:
         xmlrpc_value * valueP;
         
         cWrapper(std::string const cppvalue) {
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

     cDatetimeWrapper wrapper(cppvalue);
     
     this->instantiate(wrapper.valueP);
 }

 value_datetime::value_datetime(struct timeval const& cppvalue) {

     cDatetimeWrapper wrapper(cppvalue.tv_sec);

     this->instantiate(wrapper.valueP);
 }

 value_datetime::value_datetime(struct timespec const& cppvalue) {

     cDatetimeWrapper wrapper(cppvalue.tv_sec);

     this->instantiate(wrapper.valueP);
 }

 value_string::value_string(std::string const& cppvalue) {
     
     class cWrapper {
     public:
         xmlrpc_value * valueP;
         
         cWrapper(std::string const cppvalue) {
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


 value_bytestring::value_bytestring(std::vector<unsigned char> const& cppvalue) {

     class cWrapper {
     public:
         xmlrpc_value * valueP;
         
         cWrapper(std::vector<unsigned char> const& cppvalue) {
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
    

} // namespace


#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

#include "girerr.hpp"
using girerr::error;
#include "xmlrpc.hpp"
#include "XmlRpcCpp.h"
using namespace xmlrpc_c;

using namespace std;

//=========================================================================
//  Test Harness
//=========================================================================
// 
//  There are two styles of test in here.  The older ones are vaguely
//  inspired by Kent Beck's book on eXtreme Programming (XP) and use
//  the TEST...() macros.
//
//  But this style is not really appropriate for C++.  It's based on
//  code that explicitly tests for errors, as one would do in C.  In C++,
//  it is cumbersome to catch exceptions on every call, so we don't in
//  the new style.

//  And there's not much point in trying to count test successes and
//  failures.  Any failure is a problem, so in the new style, we just
//  quit after we recognize one (again, more in line with regular exception
//  throwing).  With exception throwing, you can't count what _didn't_
//  cause an exception, so there's no meaningful count of test successes.
//
//  To run the tests, type './cpptest'.
//  To check for memory leaks, install RedHat's 'memprof' utility, and
//  type 'memprof cpptest'.
//
//  If you add new tests to this file, please deallocate any data
//  structures you use in the appropriate fashion. This allows us to test
//  various destructor code for memory leaks.


// This is a good place to set a breakpoint.
static void 
logFailedTest(const char * const fileName, 
              unsigned int const lineNum, 
              const char * const statement) {

    ostringstream msg;

    msg << endl
        << fileName << ":" << lineNum 
        << ": expected (" << statement << ")" << endl;

    throw(error(msg.str()));
}


#define TEST(statement) \
    do { \
        if (!(statement)) \
            logFailedTest(__FILE__, __LINE__, #statement); \
    } while (0)


#define TEST_PASSED() \
    do { } while (0)

#define TEST_FAILED(reason) \
    do { \
        logFailedTest(__FILE__, __LINE__, (reason)); \
    } while (0)


#define trickToStraightenOutEmacsIndentation \
;

//=========================================================================
//  Test Suites
//=========================================================================

void 
test_fault (void) {

    // Create a new fault and perform basic operations.
    XmlRpcFault fault1 = XmlRpcFault(6, "Sample fault");
    TEST(fault1.getFaultCode() == 6);
    TEST(fault1.getFaultString() == "Sample fault");

    // Extract and examine the underlying xmlrpc_env struct.
    xmlrpc_env *env1 = fault1.getFaultEnv();
    TEST(env1 != NULL);
    TEST(env1->fault_occurred);
    TEST(env1->fault_code == 6);
    TEST(strcmp(env1->fault_string, "Sample fault") == 0);

    // Test our copy constructor.
    XmlRpcFault fault2 = fault1;
    TEST(fault2.getFaultCode() == 6);
    TEST(fault2.getFaultString() == "Sample fault");
    
    // Construct a fault from a pre-existing xmlrpc_env structure.
    xmlrpc_env env3;
    xmlrpc_env_init(&env3);
    xmlrpc_env_set_fault(&env3, 7, "Another fault");
    XmlRpcFault fault3 = XmlRpcFault(&env3);
    xmlrpc_env_clean(&env3);
    TEST(fault3.getFaultCode() == 7);
    TEST(fault3.getFaultString() == "Another fault");
    
    // Attempt to construct a fault from a fault-free xmlrpc_env.
    xmlrpc_env env4;
    xmlrpc_env_init(&env4);
    try {
        XmlRpcFault fault4 = XmlRpcFault(&env4);
        TEST_FAILED("Constructed invalid XmlRpcFault");
    } catch (XmlRpcFault& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == XMLRPC_INTERNAL_ERROR);
    }
    xmlrpc_env_clean(&env4);
}



void test_env (void) {

    // Declare these here to prevent silly compiler warnings about
    // potentially uninitialized variables.
    XmlRpcEnv env1;
    XmlRpcEnv env2;

    // Perform simple environment tests.
    TEST(!env1.hasFaultOccurred());
    xmlrpc_env_set_fault(env1, 8, "Fault 8");
    TEST(env1.hasFaultOccurred());
    XmlRpcFault fault1 = env1.getFault();
    TEST(fault1.getFaultCode() == 8);
    TEST(fault1.getFaultString() == "Fault 8");

    // Test throwIfFaultOccurred.
    try {
        env2.throwIfFaultOccurred();
        TEST_PASSED();
    } catch (XmlRpcFault& fault) {
        TEST_FAILED("We threw a fault when one hadn't occurred");
    } 
    xmlrpc_env_set_fault(env2, 9, "Fault 9");
    try {
        env2.throwIfFaultOccurred();
        TEST_FAILED("A fault occurred, and we didn't throw it");
    } catch (XmlRpcFault& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == 9);
        TEST(fault.getFaultString() == "Fault 9");
    } 
    
    // Make sure we can't get a fault if one hasn't occurred.
    XmlRpcEnv env3;
    try {
        XmlRpcFault fault3 = env3.getFault();
        TEST_FAILED("We retrieved a non-existant fault");
    } catch (XmlRpcFault& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == XMLRPC_INTERNAL_ERROR);
    }
}

void test_value (void) {
    XmlRpcEnv env;

    // Test basic reference counting behavior.
    xmlrpc_value *v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 1);
    env.throwIfFaultOccurred();
    XmlRpcValue val1 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 2);
    env.throwIfFaultOccurred();
    XmlRpcValue val2 = v;
    xmlrpc_DECREF(v);

    // Borrow a reference.
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 3);
    env.throwIfFaultOccurred();
    XmlRpcValue val3 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    xmlrpc_value *borrowed = val3.borrowReference();
    TEST(borrowed == v);

    // Make a reference.
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 4);
    env.throwIfFaultOccurred();
    XmlRpcValue val4 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    xmlrpc_value *made = val4.makeReference();
    TEST(made == v);
    xmlrpc_DECREF(made);

    // Test our default constructor.
    XmlRpcValue val5;
    TEST(val5.getBool() == false);

    // Test our type introspection.
    TEST(XmlRpcValue::makeInt(0).getType() == XMLRPC_TYPE_INT);
    
    // Test our basic data types.
    TEST(XmlRpcValue::makeInt(30).getInt() == 30);
    TEST(XmlRpcValue::makeInt(-30).getInt() == -30);
    TEST(XmlRpcValue::makeBool(true).getBool() == true);
    TEST(XmlRpcValue::makeBool(false).getBool() == false);
    TEST(XmlRpcValue::makeDateTime("19980717T14:08:55").getRawDateTime() ==
         "19980717T14:08:55");
    TEST(XmlRpcValue::makeString("foo").getString() == "foo");
    TEST(XmlRpcValue::makeString("bar", 3).getString() == "bar");
    TEST(XmlRpcValue::makeString("bar", 3).getString() == "bar");
    TEST(XmlRpcValue::makeString("a\0b").getString() == string("a\0b"));
    XmlRpcValue::makeArray().getArray();
    XmlRpcValue::makeStruct().getStruct();

    // Test Base64 values.
    const unsigned char *b64_data;
    size_t b64_len;
    XmlRpcValue val6 = XmlRpcValue::makeBase64((unsigned char*) "a\0\0b", 4);
    val6.getBase64(b64_data, b64_len);
    TEST(b64_len == 4);
    TEST(memcmp(b64_data, "a\0\0b", 4) == 0);

    // Test arrays.
    XmlRpcValue array = XmlRpcValue::makeArray();
    TEST(array.arraySize() == 0);
    array.arrayAppendItem(XmlRpcValue::makeString("foo"));
    TEST(array.arraySize() == 1);
    array.arrayAppendItem(XmlRpcValue::makeString("bar"));
    TEST(array.arraySize() == 2);
    TEST(array.arrayGetItem(0).getString() == "foo");
    TEST(array.arrayGetItem(1).getString() == "bar");

    // Test structs.
    XmlRpcValue strct = XmlRpcValue::makeStruct();
    TEST(strct.structSize() == 0);
    strct.structSetValue("foo", XmlRpcValue::makeString("fooval"));
    TEST(strct.structSize() == 1);
    strct.structSetValue("bar", XmlRpcValue::makeString("barval"));
    TEST(strct.structSize() == 2);
    TEST(strct.structHasKey("bar"));
    TEST(!strct.structHasKey("nosuch"));
    for (size_t i = 0; i < strct.structSize(); i++) {
        string key;
        XmlRpcValue value;
        strct.structGetKeyAndValue(i, key, value);
        TEST(key + "val" == value.getString());
    }
}

void test_errors (void) {
    // XXX - Test typechecks on get* methods.
    // XXX - Test typechceks on array and struct methods.
    // XXX - Test bounds checks on arrayGetItem, structGetKeyAndValue.
}


static void
testXmlRpcCpp() {
/*----------------------------------------------------------------------------
   Test the legacy XmlRpcCpp.cpp library
-----------------------------------------------------------------------------*/
    cout << "Testing XmlRpcCpp library..." << endl;

    test_fault();
    test_env();
    test_value();
    test_errors();
}



class testSuite {
/*----------------------------------------------------------------------------
   This is a base class for a test suite.  Give the suite a name
   (to be used in messages about it) and some test code via
   virtual methods suiteName() and runtests(), respectively.

   runtests() should throw either an 'error' object or an 'XmlRpcFault'
   object if the test fails.  It should throw something else if the
   test can't run.  It should throw nothing if the tests pass.

   You don't normally keep an object of this class around.  You don't
   even give it a name.  You simply refer to a literal object, like so:

     myTestSuite().run()
-----------------------------------------------------------------------------*/
public:
    void run();

    virtual void runtests() {
        throw(error("test suite does not have a runtests() method"));
    };
    virtual string suiteName() {
        return "unnamed test suite";
    }
};

void
testSuite::run() {
    try {
        cout << "Running " << suiteName() << endl;
        runtests();
    } catch (error thisError) {
        throw(error(suiteName() + string(" failed.  ") + thisError.what()));
    } catch (XmlRpcFault thisFault) {
        throw(error(suiteName() + string(" failed.  ") +
                    string("An Xmlrpc-c function threw an XmlRpcFault with "
                           "fault string ") + 
                    thisFault.getFaultString()));
    } catch (...) {
        throw(error(suiteName() + string(" failed.  ") +
                    string("It threw an unexpected type of object")));
    }
    cout << suiteName() << " tests passed." << endl;
}


class valueTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "valueTestSuite";
    }
    virtual void runtests() {

        value_int int1(7);
        value_double double1(3.14);
        value_boolean boolean1(true);
        value_datetime datetime1("19980717T14:08:55");
        value_string string1("hello world");
        
        unsigned char bytestringArray[] = {0x10, 0x11, 0x12, 0x13, 0x14};
        vector<unsigned char> 
            bytestringData(&bytestringArray[0], &bytestringArray[4]);
        value_bytestring bytestring1(bytestringData);
        
        vector<xmlrpc_c::value> arrayData;
        arrayData.push_back(int1);
        arrayData.push_back(double1);
        arrayData.push_back(boolean1);
        arrayData.push_back(datetime1);
        arrayData.push_back(string1);
        arrayData.push_back(bytestring1);
        value_array array1(arrayData);
        
        map<string, xmlrpc_c::value> structData;
        structData.insert(pair<string, xmlrpc_c::value>("the_integer", int1));
        
        value_struct struct1(structData);
        
        value_nil nil1;
    }
};

//=========================================================================
//  Test Driver
//=========================================================================

int 
main(int argc, char** argv) {

    int retval;

    if (argc-1 > 0) {
        cout << "Program takes no arguments" << endl;
        if (argv) {};  // defeat compiler warning about unused parm
    }

    bool testsPassed;

    try {
        // Add your test suites here.
        valueTestSuite().run();

        testXmlRpcCpp();

        testsPassed = true;
    } catch (error thisError) {
        cout << "Unexpected error thrown:  " << thisError.what() << endl;
        testsPassed = false;
    } catch (XmlRpcFault& fault) {
        cout << "Unexpected XML-RPC fault when running test suites." << endl
             << "Fault #" << fault.getFaultCode()
             << ": " << fault.getFaultString() << endl;
        testsPassed = false;
    } catch (...) {
        cout << "Unexpected exception when running test suites." << endl;
        testsPassed = false;
    }

    if (testsPassed) {
        cout << "PASSED" << endl;
        retval = 0;
    } else {
        cout << "FAILED" << endl;
        retval = 1;
    }
    return retval;
}

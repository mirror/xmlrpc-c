#include <iostream.h>
#include <strstream.h>
#include <stdexcept>

#include <XmlRpcCpp.h>
#include "DataType.h"
#include "XmlRpcFunction.h"


#include <vector.h>

class XmlRpcClass {
    string mClassName;
    vector<XmlRpcFunction> mFunctions;

    XmlRpcClass (const XmlRpcClass&) { XMLRPC_ASSERT(0); }
    XmlRpcClass& operator= (const XmlRpcClass&)
        { XMLRPC_ASSERT(0); return *this; }

public:
    XmlRpcClass (string class_name);

    void addFunction (const XmlRpcFunction& function);

    void printDeclaration (ostream& out);
    void printDefinition (ostream& out);
};

XmlRpcClass::XmlRpcClass (string class_name)
    : mClassName(class_name)
{
}

void XmlRpcClass::addFunction (const XmlRpcFunction& function)
{
    mFunctions.push_back(function);
}

void XmlRpcClass::printDeclaration (ostream& out)
{
    cout << "class " << mClassName << " {" << endl;
    cout << "public:" << endl;
    
    vector<XmlRpcFunction>::iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
	f->printDeclarations(cout);
    }

    cout << "};" << endl;    
}

void XmlRpcClass::printDefinition (ostream& out)
{
    vector<XmlRpcFunction>::iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
	f->printDefinitions(cout, mClassName);
    }
}


//=========================================================================
//  Bootstrapping Code
//=========================================================================
//  This code describes the system.* introspection methods.  When this
//  class is smart enough to fetch information from a server, all this
//  code will go away.

XmlRpcValue listMethodsSynopsis () {
   XmlRpcValue synopsis = XmlRpcValue::makeArray();
   XmlRpcValue signature = XmlRpcValue::makeArray();
   signature.arrayAppendItem(XmlRpcValue::makeString("array"));
   synopsis.arrayAppendItem(signature);
   return synopsis;
}

XmlRpcValue methodHelpSynopsis () {
   XmlRpcValue synopsis = XmlRpcValue::makeArray();
   XmlRpcValue signature = XmlRpcValue::makeArray();
   signature.arrayAppendItem(XmlRpcValue::makeString("string"));
   signature.arrayAppendItem(XmlRpcValue::makeString("string"));
   synopsis.arrayAppendItem(signature);
   return synopsis;
}

XmlRpcValue methodSignatureSynopsis () {
   XmlRpcValue synopsis = XmlRpcValue::makeArray();
   XmlRpcValue signature = XmlRpcValue::makeArray();
   signature.arrayAppendItem(XmlRpcValue::makeString("array"));
   signature.arrayAppendItem(XmlRpcValue::makeString("string"));
   synopsis.arrayAppendItem(signature);
   return synopsis;
}


//=========================================================================
//  function main
//=========================================================================
//  For now, just a test harness.

int main (int argc, char **argv) {

    XmlRpcClass system ("System");

    system.addFunction(XmlRpcFunction("listMethods",
				      "system.listMethods",
				      "List all supported methods.",
				      listMethodsSynopsis()));
    system.addFunction(XmlRpcFunction("methodHelp",
				      "system.methodHelp",
				      "Get the help string for a method.",
				      methodHelpSynopsis()));
    system.addFunction(XmlRpcFunction("methodSignature",
				      "system.methodSignature",
				      "Get the signature for a method.",
				      methodSignatureSynopsis()));

    
    system.printDeclaration(cout);
    system.printDefinition(cout);
}

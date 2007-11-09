#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

#include "xmlrpc-c/oldcppwrapper.hpp"

#include "DataType.hpp"
#include "XmlRpcFunction.hpp"
#include "XmlRpcClass.hpp"


XmlRpcClass::XmlRpcClass(string const& className) :
    mClassName(className) {}



XmlRpcClass::XmlRpcClass(XmlRpcClass const& c) :
    mClassName(c.mClassName),
    mFunctions(c.mFunctions) {}



XmlRpcClass&
XmlRpcClass::operator= (XmlRpcClass const& c) {

    if (this != &c) {
        this->mClassName = c.mClassName;
        this->mFunctions = c.mFunctions;
    }
    return *this;
}



void
XmlRpcClass::addFunction(XmlRpcFunction const& function) {

    mFunctions.push_back(function);
}



void
XmlRpcClass::printDeclaration(ostream &) const {

    cout << "class " << mClassName << " {" << endl;
    cout << "    XmlRpcClient mClient;" << endl;
    cout << endl;
    cout << "public:" << endl;
    cout << "    " << mClassName << " (const XmlRpcClient& client)" << endl;
    cout << "        : mClient(client) {}" << endl;
    cout << "    " << mClassName << " (const std::string& server_url)" << endl;
    cout << "        : mClient(XmlRpcClient(server_url)) {}" << endl;
    cout << "    " << mClassName << " (const " << mClassName << "& o)" << endl;
    cout << "        : mClient(o.mClient) {}" << endl;
    cout << endl;
    cout << "    " << mClassName << "& operator= (const "
         << mClassName << "& o) {" << endl;
    cout << "        if (this != &o) mClient = o.mClient;" << endl;
    cout << "        return *this;" << endl;
    cout << "    }" << endl;

    vector<XmlRpcFunction>::const_iterator f;
    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
        f->printDeclarations(cout);
    }

    cout << "};" << endl;    
}



void
XmlRpcClass::printDefinition(ostream &) const {

    vector<XmlRpcFunction>::const_iterator f;

    for (f = mFunctions.begin(); f < mFunctions.end(); ++f) {
        f->printDefinitions(cout, mClassName);
    }
}


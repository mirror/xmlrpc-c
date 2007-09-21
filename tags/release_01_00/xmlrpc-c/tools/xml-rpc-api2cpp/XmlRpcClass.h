#include <vector.h>

class XmlRpcClass {
    string mClassName;
    vector<XmlRpcFunction> mFunctions;


public:
    XmlRpcClass (string class_name);
    XmlRpcClass (const XmlRpcClass&);
    XmlRpcClass& operator= (const XmlRpcClass&);

    string className () const { return mClassName; }

    void addFunction (const XmlRpcFunction& function);

    void printDeclaration (ostream& out);
    void printDefinition (ostream& out);
};

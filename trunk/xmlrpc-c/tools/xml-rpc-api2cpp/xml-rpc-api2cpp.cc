#include <iostream.h>
#include <strstream.h>
#include <stdexcept>

#include <XmlRpcCpp.h>


//=========================================================================
//  abstract class DataType
//=========================================================================
//  Instances of DataType know how generate code fragments for manipulating
//  a specific XML-RPC data type.

class DataType {
    string mTypeName;

    DataType (const DataType&) { XMLRPC_ASSERT(0); }
    DataType& operator= (const DataType&) { XMLRPC_ASSERT(0); return *this; }

public:
    DataType (const string& type_name) : mTypeName(type_name) {}
    virtual ~DataType () {}

    // Return the name for this XML-RPC type.
    virtual string typeName () const { return mTypeName; }

    // Given a parameter position, calculate a unique base name for all
    // parameter-related variables.
    virtual string defaultParameterBaseName (int position) const;

    // Virtual functions for processing parameters.
    virtual string parameterFragment (const string& base_name) const = 0;
    virtual string inputConversionFragment (const string& base_name) const = 0;

    // Virtual functions for processing return values.
    virtual string returnTypeFragment () const = 0;
    virtual string outputConversionFragment (const string& var_name) const = 0;
};

string DataType::defaultParameterBaseName (int position) const {
    ostrstream name_stream;
    name_stream << typeName() << position;
    return string(name_stream.str());
}


//=========================================================================
//  class RawDataType
//=========================================================================
//  We want to manipulate some XML-RPC data types as XmlRpcValue objects.

class RawDataType : public DataType {
public:
    RawDataType (const string& type_name) : DataType(type_name) {}
    
    virtual string parameterFragment (const string& base_name) const;
    virtual string inputConversionFragment (const string& base_name) const;
    virtual string returnTypeFragment () const;
    virtual string outputConversionFragment (const string& var_name) const;
};

string RawDataType::parameterFragment (const string& base_name) const {
    return "XmlRpcValue " + base_name;
}

string RawDataType::inputConversionFragment (const string& base_name) const {
    return base_name;
}

string RawDataType::returnTypeFragment () const {
    return "XmlRpcValue";
}

string RawDataType::outputConversionFragment (const string& var_name) const {
    return var_name;
}


//=========================================================================
//  class SimpleDataType
//=========================================================================
//  Other types can be easily converted to and from a single native type.

class SimpleDataType : public DataType {
    string mNativeType;
    string mMakerFunc;
    string mGetterFunc;

public:
    SimpleDataType (const string& type_name,
		    const string& native_type,
		    const string& maker_func,
		    const string& getter_func);

    virtual string parameterFragment (const string& base_name) const;
    virtual string inputConversionFragment (const string& base_name) const;
    virtual string returnTypeFragment () const;
    virtual string outputConversionFragment (const string& var_name) const;
};

SimpleDataType::SimpleDataType (const string& type_name,
				const string& native_type,
				const string& maker_func,
				const string& getter_func)
    : DataType(type_name),
      mNativeType(native_type),
      mMakerFunc(maker_func),
      mGetterFunc(getter_func)
{
}

string SimpleDataType::parameterFragment (const string& base_name) const {
    return mNativeType + " " + base_name;
}

string SimpleDataType::inputConversionFragment (const string& base_name) const
{
    return mMakerFunc + "(" + base_name + ")";
}

string SimpleDataType::returnTypeFragment () const {
    return mNativeType; 
}

string SimpleDataType::outputConversionFragment (const string& var_name) const
{
    return mGetterFunc + "(" + var_name + ")";
}


//=========================================================================
//  function findDataType
//=========================================================================
//  Given the name of an XML-RPC data type, try to find a corresponding
//  DataType object.

SimpleDataType intType    ("int", "XmlRpcValue::int32",
			   "XmlRpcValue::makeInt",
			   "XmlRpcValue::getInt");
SimpleDataType boolType   ("bool", "bool",
			   "XmlRpcValue::makeBool",
			   "XmlRpcValue::getBool");
SimpleDataType doubleType ("double", "double",
			   "XmlRpcValue::makeDouble",
			   "XmlRpcValue::getDouble");
SimpleDataType stringType ("string", "string",
			   "XmlRpcValue::makeString",
			   "XmlRpcValue::getString");

RawDataType dateTimeType  ("dateTime");
RawDataType base64Type    ("base64");
RawDataType structType    ("struct");
RawDataType arrayType     ("array");

const DataType& findDataType (const string& name) {
    if (name == "int" || name == "i4")
	return intType;
    else if (name == "bool")
	return boolType;
    else if (name == "double")
	return doubleType;
    else if (name == "string")
	return stringType;
    else if (name == "dateTime")
	return dateTimeType;
    else if (name == "base64")
	return base64Type;
    else if (name == "struct")
	return structType;
    else if (name == "array")
	return arrayType;
    else
	throw domain_error("Unknown XML-RPC type " + name);
    
    // This code should never be executed.
    XMLRPC_ASSERT(0);
    return intType;
}


//=========================================================================
//  class XmlRpcFunction
//=========================================================================
//  Contains everything we know about a given server function, and knows
//  how to print local bindings.

class XmlRpcFunction {
    string mFunctionName;
    string mHelp;
    XmlRpcValue mSynopsis;

    XmlRpcFunction (const XmlRpcFunction&) { XMLRPC_ASSERT(0); }
    XmlRpcFunction& operator= (const XmlRpcFunction&)
        { XMLRPC_ASSERT(0); return *this; }

public: 
    XmlRpcFunction(const string& function_name, const string& help,
		   XmlRpcValue& synopsis);
    
    void printDeclarations (ostream& out);
    void printDefinitions (ostream& out, const string& className);

private:
    void printDeclaration (ostream& out, size_t synopsis_index);
    void printDefinition (ostream& out,
			  const string& className,
			  size_t synopsis_index);

    const DataType& returnType (size_t synopsis_index);
    size_t parameterCount (size_t synopsis_index);
    const DataType& parameterType (size_t synopsis_index,
				   size_t parameter_index);
};

XmlRpcFunction::XmlRpcFunction(const string& function_name,
			       const string& help,
			       XmlRpcValue& synopsis)
    : mFunctionName(function_name), mHelp(help), mSynopsis(synopsis)
{
}

void XmlRpcFunction::printDeclarations (ostream& out) {
    // XXX - Do a sloppy job of printing documentation.
    out << endl << "    /* " << mHelp << " */" << endl;

    // Print each declaration.
    size_t end = mSynopsis.arraySize();
    for (size_t i = 0; i < end; i++)
	printDeclaration(out, i);
}

void XmlRpcFunction::printDefinitions (ostream& out, const string& className) {
    size_t end = mSynopsis.arraySize();
    for (size_t i = 0; i < end; i++) {
	out << endl;
	printDefinition(out, className, i);
    }
}

void XmlRpcFunction::printDeclaration (ostream& out, size_t synopsis_index) {

    // Print the first part of our function declaration.
    const DataType& rtype (returnType(synopsis_index));
    out << "    " << rtype.returnTypeFragment() << " "
	<< mFunctionName << " (";

    // Print the parameter declarations.
    size_t end = parameterCount(synopsis_index);
    bool first = true;
    for (size_t i = 0; i < end; i++) {
	if (first)
	    first = false;
	else
	    out << ", ";

	const DataType& ptype (parameterType(synopsis_index, i));
	string basename = ptype.defaultParameterBaseName(i + 1);
	out << ptype.parameterFragment(basename);
    }

    // Print the last part of our function declaration.
    out << ");" << endl;
}

void XmlRpcFunction::printDefinition (ostream& out,
				      const string& className,
				      size_t synopsis_index)
{
    cout << "Definition #" << synopsis_index << endl;
}

const DataType& XmlRpcFunction::returnType (size_t synopsis_index) {
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    return findDataType(func_synop.arrayGetItem(0).getString());
}

size_t XmlRpcFunction::parameterCount (size_t synopsis_index) {
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    size_t size = func_synop.arraySize();
    if (size < 1)
	throw domain_error("Synopsis contained no items");
    return size - 1;
}

const DataType& XmlRpcFunction::parameterType (size_t synopsis_index,
					       size_t parameter_index)
{
    XmlRpcValue func_synop = mSynopsis.arrayGetItem(synopsis_index);
    XmlRpcValue param = func_synop.arrayGetItem(parameter_index + 1);
    return findDataType(param.getString());
}


//=========================================================================
//  function main
//=========================================================================
//  For now, just a test harness.

int main (int argc, char **argv) {
    
    // Some information about our sample function.
    string function_name = "sumAndDifference";
    string help = "Add and subtract two numbers.";

    // Build a two-item synopsis.
    XmlRpcValue synopsis = XmlRpcValue::makeArray();
    XmlRpcValue synopsis1 = XmlRpcValue::makeArray();
    synopsis1.arrayAppendItem(XmlRpcValue::makeString("struct"));
    synopsis1.arrayAppendItem(XmlRpcValue::makeString("int"));
    synopsis1.arrayAppendItem(XmlRpcValue::makeString("int"));
    synopsis.arrayAppendItem(synopsis1);
    XmlRpcValue synopsis2 = XmlRpcValue::makeArray();
    synopsis2.arrayAppendItem(XmlRpcValue::makeString("struct"));
    synopsis2.arrayAppendItem(XmlRpcValue::makeString("double"));
    synopsis2.arrayAppendItem(XmlRpcValue::makeString("double"));
    synopsis.arrayAppendItem(synopsis2);

    // Build a function object.
    XmlRpcFunction function (function_name, help, synopsis);

    // Print all declarations.
    function.printDeclarations(cout);

    // Print all definitions.
    function.printDefinitions(cout, "Sample");
}

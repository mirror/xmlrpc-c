#include <string>

class DataType {
    std::string mTypeName;

    DataType (const DataType&) { XMLRPC_ASSERT(0); }
    DataType& operator= (const DataType&) { XMLRPC_ASSERT(0); return *this; }

public:
    DataType (const std::string& type_name) : mTypeName(type_name) {}
    virtual ~DataType () {}

    // Return the name for this XML-RPC type.
    virtual std::string typeName () const { return mTypeName; }

    // Given a parameter position, calculate a unique base name for all
    // parameter-related variables.
    virtual std::string defaultParameterBaseName (int position) const;

    // Virtual functions for processing parameters.
    virtual std::string parameterFragment (const string& base_name) const = 0;
    virtual std::string inputConversionFragment (const string& base_name) 
        const = 0;

    // Virtual functions for processing return values.
    virtual std::string returnTypeFragment () const = 0;
    virtual std::string outputConversionFragment (const string& var_name) 
        const = 0;
};

const DataType& findDataType (const std::string& name);

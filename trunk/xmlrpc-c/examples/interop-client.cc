// Run various interop test cases against a list of servers.
// This code is incomplete.

#include <iostream>
#include <fstream>
#include <stdexcept>

using namespace std;

#include <XmlRpcCpp.h>

#include "InteropEchoProxy.h"

#define NAME           "XML-RPC C++ Interop Client"
#define VERSION        "0.1"

//=========================================================================
//  class TestResults
//=========================================================================

class TestResults {
    string m_server_name;
    string m_server_url;
    XmlRpcValue m_toolkit_info;
    XmlRpcValue m_test_results;

public:
    TestResults(string server_name, string server_url);
    
};


//=========================================================================
//  Top-level Application Code
//=========================================================================

static void
run_interop_tests(const string& server_url_file,
		  const string& output_html_file)
{
    ifstream urls(server_url_file.c_str());
    ofstream out(output_html_file.c_str());

    while (!urls.eof()) {
	string url_info;
	getline(urls, url_info);
	size_t comma = url_info.find(',');
	if (comma == string::npos)
	    throw domain_error("Lines of " + server_url_file +
			       " must be of the form \"name,url\"");
	string server_name(url_info, 0, comma);
	string server_url(url_info, comma + 1);

	cout << "Name: " << server_name << endl;
	cout << "URL: " << server_url << endl << endl;
    }
}
			      

//=========================================================================
//  main() and Support Functions
//=========================================================================

// Print out a usage message.
static void usage (void)
{
    cerr << "Usage: interop-client <server-url-file> <output-html-file>";
    cerr << endl;
    exit(1);
}

int main (int argc, char **argv) {
    int status = 0;

    // Parse our command-line arguments.
    if (argc != 3)
	usage();
    string server_url_file(argv[1]);
    string output_html_file(argv[2]);

    // Start up our client library.
    XmlRpcClient::Initialize(NAME, VERSION);

    // Call our implementation, and watch out for faults.
    try {
	run_interop_tests(server_url_file, output_html_file);
    } catch (XmlRpcFault& fault) {
	cerr << argv[0] << ": XML-RPC fault #" << fault.getFaultCode()
	     << ": " << fault.getFaultString() << endl;
	status = 1;
    } catch (logic_error& err) {
	cerr << argv[0] << ": " << err.what() << endl;
	status = 1;
    } catch (...) {
	cerr << argv[0] << ": Unknown exception" << endl;
	status = 1;
    }

    // Shut down our client library.
    XmlRpcClient::Terminate();

    return status;
}

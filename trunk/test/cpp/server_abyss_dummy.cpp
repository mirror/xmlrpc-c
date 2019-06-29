/*=============================================================================
                               server_abyss_dummy
===============================================================================
  This is a substitute for server_abyss.cpp, for use in a test program that is
  not linked with the Abyss XML-RPC server libraries.

  It simply passes the test.
=============================================================================*/

#include <string>
#include <iostream>

#include "tools.hpp"
#include "server_abyss.hpp"

using namespace std;

string
serverAbyssTestSuite::suiteName() {
    return "serverAbyssTestSuite";
}


void
serverAbyssTestSuite::runtests(unsigned int const indentation) {

    cout << string((indentation+1)*2, ' ') 
         << "Running dummy test." << endl;
}

/*=============================================================================
                                  abyss_dummy
===============================================================================
  This is a substitute for abyss.cpp, for use in a test program that is
  not linked with the Abyss libraries.

  It simply passes the test.
=============================================================================*/

#include <string>
#include <iostream>

#include "tools.hpp"
#include "abyss.hpp"

using namespace std;

string
abyssTestSuite::suiteName() {
    return "abyssTestSuite";
}


void
abyssTestSuite::runtests(unsigned int const indentation) {

    cout << string((indentation+1)*2, ' ') 
         << "Running dummy test." << endl;
}

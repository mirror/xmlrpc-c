#include <stdexcept>

using namespace std;

#ifndef _WIN32
  /* Without _WIN32, <xmlrpc-c/abyss.h> does not declare ChanSwitchWinCreate */
  #error This module compiles only for a Windows target.
#endif

#include "xmlrpc-c/abyss.h"

#include "xmlrpc-c/AbyssChanSwitchWin.hpp"




namespace xmlrpc_c {



AbyssChanSwitchWin::AbyssChanSwitchWin(unsigned short const listenPortNum) {

    const char * error;

    ChanSwitchWinCreate(listenPortNum, &this->_cChanSwitchP, &error);

    if (error)
        throw runtime_error(error);
}



}  // namespace

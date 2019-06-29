#include <stdexcept>

using namespace std;

#ifdef _WIN32
  /* With _WIN32, <xmlrpc-c/abyss.h> does not declare ChanSwitchUnixCreate */
  #error This module does not compile for a Windows target.
#endif

#include "xmlrpc-c/abyss.h"

#include "xmlrpc-c/AbyssChanSwitchUnix.hpp"




namespace xmlrpc_c {



AbyssChanSwitchUnix::AbyssChanSwitchUnix(unsigned short const listenPortNum) {

    const char * error;

    ChanSwitchUnixCreate(listenPortNum, &this->_cChanSwitchP, &error);

    if (error)
        throw runtime_error(error);
}



}  // namespace

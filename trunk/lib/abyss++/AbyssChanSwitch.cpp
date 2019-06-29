#include <stdexcept>

using namespace std;

#include "xmlrpc-c/abyss.h"

#include "xmlrpc-c/AbyssChanSwitch.hpp"



namespace xmlrpc_c {



AbyssChanSwitch::AbyssChanSwitch() {

    this->_cChanSwitchP = NULL;
}



AbyssChanSwitch::~AbyssChanSwitch() {

    if (this->_cChanSwitchP)
        ChanSwitchDestroy(this->_cChanSwitchP);
}



TChanSwitch *
AbyssChanSwitch::cChanSwitchP() const {

    return this->_cChanSwitchP;
}



}  // namespace

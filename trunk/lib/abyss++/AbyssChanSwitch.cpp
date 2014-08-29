#include <stdexcept>

using namespace std;

#include <xmlrpc-c/abyss.h>

#include "xmlrpc-c/AbyssChanSwitch.hpp"



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



AbyssChanSwitchUnix::AbyssChanSwitchUnix(unsigned short const listenPortNum) {

    const char * error;

    ChanSwitchUnixCreate(listenPortNum, &this->_cChanSwitchP, &error);

    if (error)
        throw runtime_error(error);
}




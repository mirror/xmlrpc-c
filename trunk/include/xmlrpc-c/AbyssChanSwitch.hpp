#ifndef ABYSS_CHANSWITCH_HPP_INCLUDED
#define ABYSS_CHANSWITCH_HPP_INCLUDED

#include <xmlrpc-c/abyss.h>

class AbyssChanSwitch {

public:
    ~AbyssChanSwitch();

    TChanSwitch *
    cChanSwitchP() const;

protected:
    AbyssChanSwitch();

    TChanSwitch * _cChanSwitchP;
        // NULL when derived class constructor has not yet created the
        // TChanSwitch object.
};

class AbyssChanSwitchUnix : public AbyssChanSwitch {

public:
    AbyssChanSwitchUnix(unsigned short const listenPortNum);
};

#endif

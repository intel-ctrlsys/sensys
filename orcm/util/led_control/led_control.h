/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#ifdef GTEST_MOCK_TESTING
    #define PRIVATE public
#else
    #define PRIVATE private
#endif

#include "orcm/util/led_control/ipmicmd_wrapper.h"

const unsigned char IPMI_SESSION_AUTHTYPE_PASSWORD_ = 0x04;
const unsigned char IPMI_PRIV_LEVEL_USER_ = 0x02;

enum LedState{
    LED_OFF = 0,
    LED_TEMPORARY_ON,
    LED_INDEFINITE_ON
};

enum ChassisStatusIndex{
    CURRENT_POWER = 0,
    LAST_POWER_EVENT,
    MISC_CHASSIS,
    FRONT_PANEL_BUTTONS
};

// This class provides an interface to turn ON (indefinite or temporary) and
// OFF the Chassis ID LED through IPMI.
class LedControl{
    public:
        // Constructor for in-band IPMI operations (local BMC communication).
        LedControl();

        // Constructor for OOB IPMI operations (BMC communication through LAN).
        //
        // @param
        //      hostname: BMC hostname/IP address
        //      user: BMC user
        //      pass: BMC password
        LedControl(const char *hostname, const char *user, const char *pass, int auth, int priv);

        // Destructor
        virtual ~LedControl();

        // Turn Chassis ID LED ON indefinitely.
        //
        // @return 0 if succeed, <0 in case of IPMI failure
        int enableChassisID();

        // Turn Chassis ID LED ON for a given period of time.
        //
        // @param
        //      seconds: Seconds to turn the LED OK.
        //
        // @return 0 if succeed, <0 in case of IPMI failure
        int enableChassisID(unsigned char seconds);

        // Turn Chassis ID LED OFF indefinitely.
        //
        // @return 0 if succeed, <0 in case of IPMI failure
        int disableChassisID();

        // Retreive Chassis ID LED status.
        //
        // @return LED_OFF, LED_TEMPORARY_ON or LED_INDEFINITE_ON. Anything else
        //         shall be considered an error.
        int getChassisIDState();

    private:
        bool remote_node;
        char* hostname;
        char* user;
        char* pass;
        int auth;
        int priv;
        int setChassisID(int, unsigned char);
        int ipmiCmdOperation(unsigned short, unsigned char*, int, unsigned char*,
                    int *, unsigned char *);

    protected:
        IPMICmdWrapper *ipmi;
};

#endif /* LED_CONTROL_H */

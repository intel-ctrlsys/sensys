/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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
        LedControl(const char *hostname, const char *user, const char *pass);

        // Destructor
        ~LedControl();

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
        int setChassisID(unsigned char);
        int ipmiCmdOperation(unsigned short, unsigned char*, int, unsigned char*,
                    int *, unsigned char *);

    private:
        bool remote_node;
        char* hostname;
        char* user;
        char* pass;
};

#endif /* LED_CONTROL_H */

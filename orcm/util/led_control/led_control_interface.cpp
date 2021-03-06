/*
 * Copyright (c) 2016-2017 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/util/led_control/led_control.h"

LedControl *lc = 0;

extern "C" {
    #include "orcm/util/led_control/led_control_interface.h"

    void init_led_control(char* hostname, char* user, char* pass, int auth, int priv){
        lc = new LedControl(hostname, user, pass, auth, priv);
    }

    int get_chassis_id_state(){
        return lc->getChassisIDState();
    }

    int disable_chassis_id(){
        return lc->disableChassisID();
    }

    int enable_chassis_id(){
        return lc->enableChassisID();
    }

    int enable_chassis_id_with_timeout(int seconds){
        return lc->enableChassisID(seconds);
    }

    void fini_led_control(){
        delete lc;
    }
}

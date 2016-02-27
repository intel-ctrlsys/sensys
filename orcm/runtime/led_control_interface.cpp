/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "led_control.h"

LedControl *lc = 0;

extern "C" {
    #include "led_control_interface.h"

    void init_led_control(){
        lc = new LedControl();
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

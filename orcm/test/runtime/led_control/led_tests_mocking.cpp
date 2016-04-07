/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>
#include <string.h>
#include <ipmicmd.h>

#include "led_tests_mocking.h"
#include "orcm/runtime/led_control.h"

int ipmi_open_session_count = 0;
bool is_remote_node = false;
bool is_supported = true;

extern "C" {

    int __wrap_ipmi_cmd(unsigned short cmd, unsigned char *pdata, int sdata,
            unsigned char *presp, int *sresp, unsigned char *pcc, char fdebugcmd){
            static unsigned char chassis_id_state = LED_OFF;

        int misc_chassis_state = 0x00;
        switch(cmd){
            case CHASSIS_STATUS:
                if (is_supported){
                    misc_chassis_state = 0x40;
                }
                presp[MISC_CHASSIS] = misc_chassis_state | (chassis_id_state << 4);
                *sresp = 4;
                *pcc = 0;
                break;
            case CHASSIS_IDENTIFY:
                if (sdata == 2 && pdata[1] == 1)
                    chassis_id_state = LED_INDEFINITE_ON;
                else if (sdata == 1 && pdata[0] == 0)
                    chassis_id_state = LED_OFF;
                else if (sdata == 1 && pdata[0] != 0)
                    chassis_id_state = LED_TEMPORARY_ON;
                *sresp = 0;
                *pcc = 0;
                break;
        }
        ++ipmi_open_session_count;
        return 0;
    }

    int __wrap_ipmi_close(void){
        --ipmi_open_session_count;
        return 0;
    }

    int __wrap_set_lan_options(char *node, char *user, char *pswd, int auth,
            int priv, int cipher, void *addr, int addr_len){
        is_remote_node = true;
        if (!strcmp("", user))
            return 1;
        return 0;
    }
}

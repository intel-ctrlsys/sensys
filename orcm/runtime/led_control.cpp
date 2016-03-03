/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ipmicmd.h>

#include "led_control.h"

#define MAX_BUFF_SIZE 32

using namespace std;

LedControl::LedControl(){
    this->remote_node = false;
    this->hostname = NULL;
    this->user = NULL;
    this->pass = NULL;
}

LedControl::LedControl(const char *hostname, const char *user, const char *pass){
    this->remote_node = true;
    this->hostname = strdup(hostname);
    this->user = strdup(user);
    this->pass = strdup(pass);
}

LedControl::~LedControl(){
    if (this->hostname)
        free(this->hostname);
    if (this->user)
        free(this->user);
    if (this->pass)
        free(this->pass);
}

int LedControl::ipmiCmdOperation(unsigned short cmd, unsigned char *buff_in,
        int in_size, unsigned char *buff_out, int *out_size,
        unsigned char *ccode){
    int ret = 0;
    if (this->remote_node){
        ret = set_lan_options(hostname, user, pass, IPMI_SESSION_AUTHTYPE_PASSWORD,
            IPMI_PRIV_LEVEL_ADMIN, 3, NULL, 0);
        if (ret)
            return ret;
    }
    ret = ipmi_cmd(cmd, buff_in, in_size, buff_out, out_size, ccode, 0);
    ipmi_close();
    return ret;
}

int LedControl::getChassisIDState(){
    unsigned char buff_out[MAX_BUFF_SIZE];
    int out_size = sizeof(buff_out);
    unsigned char c_code;

    ipmiCmdOperation(CHASSIS_STATUS, NULL, 0, buff_out, &out_size, &c_code);

    return (buff_out[MISC_CHASSIS] >> 4) & 3;
}

int LedControl::setChassisID(unsigned char value){
    unsigned char buff_in[MAX_BUFF_SIZE];
    int in_size = 0;
    int out_size = 0;
    unsigned char c_code;

    buff_in[0] = value;
    buff_in[1] = 1;
    in_size = (value == LED_INDEFINITE_ON) ? 2 : 1;

    return ipmiCmdOperation(CHASSIS_IDENTIFY, buff_in, in_size, NULL, &out_size, &c_code);
}

int LedControl::disableChassisID(){
    return this->setChassisID(LED_OFF);
}

int LedControl::enableChassisID(unsigned char seconds){
    return this->setChassisID(seconds);
}

int LedControl::enableChassisID(){
    return this->setChassisID(LED_INDEFINITE_ON);
}

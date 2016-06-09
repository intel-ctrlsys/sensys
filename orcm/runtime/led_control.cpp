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
#ifdef HAVE_LED_CONTROL_SUPPORT
    #include <ipmicmd.h>
#endif

#include "led_control.h"

#define MAX_BUFF_SIZE 32

using namespace std;

LedControl::LedControl(){
    this->remote_node = false;
    this->hostname = NULL;
    this->user = NULL;
    this->pass = NULL;
    this->auth = IPMI_SESSION_AUTHTYPE_PASSWORD;
    this->priv = IPMI_PRIV_LEVEL_USER;
}

LedControl::LedControl(const char *hostname, const char *user, const char *pass,
        int auth, int priv){
    this->remote_node = true;
    this->hostname = strdup(hostname);
    this->user = strdup(user);
    this->pass = strdup(pass);
    this->auth = auth;
    this->priv = priv;
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
#ifdef HAVE_LED_CONTROL_SUPPORT
    int ret = 0;
    if (this->remote_node){
        ret = set_lan_options(hostname, user, pass, auth, priv, 3, NULL, 0);
        if (ret)
            return ret;
    }
    ret = ipmi_cmd(cmd, buff_in, in_size, buff_out, out_size, ccode, 0);
    ipmi_close();
    return ret;
#else
    return -1;
#endif
}

int LedControl::getChassisIDState(){
    unsigned char buff_out[MAX_BUFF_SIZE];
    int out_size = sizeof(buff_out);
    unsigned char c_code;
    int is_supported = 0;

#ifdef HAVE_LED_CONTROL_SUPPORT
    ipmiCmdOperation(CHASSIS_STATUS, NULL, 0, buff_out, &out_size, &c_code);

    is_supported = (buff_out[MISC_CHASSIS] >> 4) & 4;
    if (!is_supported){
        return -1;
    }
    return (buff_out[MISC_CHASSIS] >> 4) & 3;
#else
    return -1;
#endif
}

int LedControl::setChassisID(int action, unsigned char seconds){
    unsigned char buff_in[MAX_BUFF_SIZE];
    unsigned char buff_out[MAX_BUFF_SIZE];
    int in_size = 0;
    int out_size = 0;
    unsigned char c_code;

    buff_in[0] = seconds;
    buff_in[1] = 1;
    in_size = (action == LED_INDEFINITE_ON) ? 2 : 1;

#ifdef HAVE_LED_CONTROL_SUPPORT
    return ipmiCmdOperation(CHASSIS_IDENTIFY, buff_in, in_size, buff_out, &out_size, &c_code);
#else
    return -1;
#endif
}

int LedControl::disableChassisID(){
    return this->setChassisID(LED_OFF, 0);
}

int LedControl::enableChassisID(unsigned char seconds){
    return this->setChassisID(LED_TEMPORARY_ON, seconds);
}

int LedControl::enableChassisID(){
    return this->setChassisID(LED_INDEFINITE_ON, 0);
}

/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMICMD_WRAPPER_H
#define IPMICMD_WRAPPER_H

#include "orcm/util/dlopen_helper.h"

const unsigned char CHASSIS_STATUS_CMD = 0x01;
const unsigned char CHASSIS_IDENTIFY_CMD = 0x04;

typedef int (*ipmi_cmd_func_t)(unsigned short cmd, unsigned char *pdata, int sdata,
        unsigned char *presp, int *sresp, unsigned char *pcc, char fdebugcmd);

typedef int (*set_lan_options_func_t)(char *node, char *user, char *pswd, int auth,
        int priv, int cipher, void *addr, int addr_len);

typedef int (*ipmi_close_func_t)(void);

class IPMICmdWrapper : public DlopenHelper
{
public:
    IPMICmdWrapper(void);
    virtual ~IPMICmdWrapper(void);

    int ipmiCommand(unsigned short cmd, unsigned char *pdata,  int sdata,
            unsigned char *presp, int *sresp, unsigned char *pcc, char fdebugcmd);

    int setLanOptions(char *node, char *user, char *pswd, int auth, int priv,
                    int cipher, void *addr, int addr_len);
    int ipmiClose(void);

protected:
    ipmi_cmd_func_t _ipmi_cmd;
    set_lan_options_func_t _set_lan_options;
    ipmi_close_func_t _ipmi_close;
};

#endif // IPMICMD_WRAPPER_H

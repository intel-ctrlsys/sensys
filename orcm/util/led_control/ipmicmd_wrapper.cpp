/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <dlfcn.h>
#include <string>

#include "orcm/util/led_control/ipmicmd_wrapper.h"

using namespace std;

#ifdef HAVE_LED_CONTROL_SUPPORT
IPMICmdWrapper::IPMICmdWrapper(void) : DlopenHelper("libipmiutil.so"), _ipmi_cmd(NULL),
        _set_lan_options(NULL), _ipmi_close(NULL)
{
    if (_handler)
    {
        _ipmi_cmd = (ipmi_cmd_func_t)dlsym(_handler, "ipmi_cmd");
        _set_lan_options = (set_lan_options_func_t)dlsym(_handler, "set_lan_options");
        _ipmi_close = (ipmi_close_func_t)dlsym(_handler, "ipmi_close");
    }
}
#else
IPMICmdWrapper::IPMICmdWrapper(void) : _ipmi_cmd(NULL),
        _set_lan_options(NULL), _ipmi_close(NULL){}
#endif

IPMICmdWrapper::~IPMICmdWrapper(void){}

int IPMICmdWrapper::ipmiCommand(unsigned short cmd, unsigned char *pdata, int sdata,
        unsigned char *presp, int *sresp, unsigned char *pcc, char fdebugcmd)
{
    if (!_ipmi_cmd)
        return -1;
    return (*_ipmi_cmd)(cmd, pdata, sdata, presp, sresp, pcc, fdebugcmd);
}

int IPMICmdWrapper::setLanOptions(char *node, char *user, char *pswd, int auth,
        int priv, int cipher, void *addr, int addr_len)
{
    if (!_set_lan_options)
        return -1;
    return (*_set_lan_options)(node, user, pswd, auth, priv, cipher, addr, addr_len);
}

int IPMICmdWrapper::ipmiClose(void)
{
    if (!_ipmi_close)
        return -1;
    return (*_ipmi_close)();
}

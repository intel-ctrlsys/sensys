/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiutilAgent_mocks.h"
#include <iostream>

sensor_ipmi_sel_mocked_functions sel_mocking;
std::map<supportedMocks, mockManager> mocks;

static int dispatchIpmiResponse(mockStates state)
{
    switch (state)
    {
        case FAILURE:
            return -2; //send to BMC failed
        default:
            return 0;
    }
}

extern "C"
{
    void test_error_sink_c(int level, const char* msg)
    {
        sel_mocking.test_error_sink(level, std::string(msg));
    }

    void test_ras_event_c(const char* event, const char* hostname, void* user_object)
    {
        (void)user_object;
        sel_mocking.test_ras_event_sink(std::string(event), std::string(hostname));
    }

    int __wrap_set_lan_options(char* node,
                               char* user,
                               char* pswd,
                               int auth,
                               int priv,
                               int cipher,
                               void* addr,
                               int addr_len)
    {
        mockStates state = (mockStates) mocks[SET_LAN_OPTIONS].nextMockState();
        if (NO_MOCK == state)
            return __real_set_lan_options(node, user, pswd, auth, priv, cipher, addr, addr_len);

        if (LEGACY == state)
            return sel_mocking.mock_set_lan_options(node, user, pswd, auth, priv, cipher, addr, addr_len);

        return dispatchIpmiResponse(state);
    }

    int __wrap_ipmi_cmd_mc(unsigned short icmd,
                           unsigned char *pdata,
                           int sdata,
                           unsigned char *presp,
                           int *sresp,
                           unsigned char *pcc,
                           char fdebugcmd)
    {
        mockStates state = (mockStates) mocks[IPMI_CMD_MC].nextMockState();
        if (NO_MOCK == state)
            return __real_ipmi_cmd_mc(icmd, pdata, sdata, presp, sresp, pcc, fdebugcmd);

        return dispatchIpmiResponse(state);
    }

    int __wrap_ipmi_cmd(unsigned short icmd,
                        unsigned char *pdata,
                        int sdata,
                        unsigned char *presp,
                        int *sresp,
                        unsigned char *pcc,
                        char fdebugcmd)
    {
        mockStates state = (mockStates) mocks[IPMI_CMD].nextMockState();
        if (NO_MOCK == state)
            return __real_ipmi_cmd(icmd, pdata, sdata, presp, sresp, pcc, fdebugcmd);

        if (LEGACY == state)
            return sel_mocking.mock_ipmi_cmd(icmd, pdata, sdata, presp, sresp, pcc, fdebugcmd);

        if (RETURN_FRU_AREA_1 == state) {
            int counter = 0;
            presp[0] = 0x02;
            presp[1] = 0x03;
            presp[2] = 0x0F;
        }

        if (RETURN_FRU_AREA_2 == state) {
            int counter = 0;
            presp[0] = 0x03;
            presp[1] = 0x04;
            presp[2] = 0x0F;
        }

        return dispatchIpmiResponse(state);
    }

    int __wrap_get_sdr_cache(unsigned char **pret)
    {
        mockStates state = (mockStates) mocks[GET_SDR_CACHE].nextMockState();
        if (NO_MOCK == state)
            return __real_get_sdr_cache(pret);

        return dispatchIpmiResponse(state);
    }

    int __wrap_find_sdr_next(unsigned char *psdr, unsigned  char *pcache, unsigned short int id)
    {
        mockStates state = (mockStates) mocks[GET_SDR_CACHE].nextMockState();
        if (NO_MOCK == state)
            return __real_find_sdr_next(psdr, pcache, id);

        return -1;
    }

    int __wrap_rename(const char* old_name, const char* new_name)
    {
        return sel_mocking.mock_rename(old_name, new_name);
    }

    int __wrap_ipmi_close()
    {
        return sel_mocking.mock_ipmi_close();
    }

    void __wrap_opal_output_verbose(int level, int output_id, const char* format, ...)
    {
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        sel_mocking.mock_opal_output_verbose(level, output_id, (const char*)buffer);
    }

    int __wrap_getlogin_r(char* user, size_t user_size)
    {
        return sel_mocking.mock_getlogin_r(user, user_size);
    }

    __uid_t __wrap_geteuid()
    {
        return sel_mocking.mock_geteuid();
    }

    char *__wrap_strdup(const char *s1)
    {
        mockStates state = (mockStates) mocks[STRDUP].nextMockState();
        if (NO_MOCK == state)
            return __real_strdup(s1);

        return NULL;
    }

    int __wrap_ipmi_cmdraw(unsigned char cmd,
                           unsigned char netfn,
                           unsigned char sa,
                           unsigned char bus,
                           unsigned char lun,
                           unsigned char *pdata,
                           int sdata,
                           unsigned char *presp,
                           int *sresp,
                           unsigned char *pcc,
                           char fdebugcmd)
    {
        mockStates state = (mockStates) mocks[IPMI_CMDRAW].nextMockState();
        if (NO_MOCK == state)
            return __real_ipmi_cmdraw(cmd, netfn, sa, bus, lun, pdata, sdata, presp, sresp, pcc, fdebugcmd);

        if (RETRIEVE_BUFFER_1 == state)
        {
            presp[0] = 0x06;
            presp[1] = 0xc1;
            presp[2] = 0x34;
            presp[3] = 0xb5;
            presp[4] = 0x17;
            presp[5] = 0xcb;
            presp[6] = 0x98;
            *sresp = 7;
            *pcc = 0;
            return 0;
        }

        if (SUCCESS == state)
        {
            *pcc = 0;
            *sresp = 0;
            return 0;
        }

        return dispatchIpmiResponse(state);
    }

}

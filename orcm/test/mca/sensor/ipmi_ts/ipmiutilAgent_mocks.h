/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIUTILAGENT_MOCKS_H
#define IPMIUTILAGENT_MOCKS_H

#include <queue>

class mockManager
{
private:
    std::queue<int> pendingMocks;
public:
    inline void restartMock()
    {
        pendingMocks = std::queue<int>();
    }

    inline int nextMockState()
    {
        if (pendingMocks.empty())
            return 0;

        int retValue = pendingMocks.front();
        pendingMocks.pop();

        return retValue;
    }

    inline void pushState(int s)
    {
        pendingMocks.push(s);
    }
};

enum supportedMocks
{
    SET_LAN_OPTIONS,
    IPMI_CMD_MC,
    IPMI_CMD,
    GET_SDR_CACHE,
};

static std::map<supportedMocks, mockManager> mocks;

enum mockStates
{
    NO_MOCK = 0,
    SUCCESS,
    FAILURE
};

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
    // MOCK FOR set_lan_options
    extern int __real_set_lan_options(char*, char*, char*, int, int, int, void*, int);
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

        return dispatchIpmiResponse(state);
    }

    // MOCK FOR ipmi_cmd_mc
    extern int __real_ipmi_cmd_mc(unsigned short, unsigned char*, int, unsigned char*, int*, unsigned char*, char);
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

    // MOCK FOR ipmi_cmd
    extern int __real_ipmi_cmd(unsigned short, unsigned char*, int, unsigned char*, int*, unsigned char*, char);
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

        return dispatchIpmiResponse(state);
    }

    // MOCK FOR get_sdr_cache
    extern int __real_get_sdr_cache(unsigned char **pret);
    int __wrap_get_sdr_cache(unsigned char **pret)
    {
        mockStates state = (mockStates) mocks[GET_SDR_CACHE].nextMockState();
        if (NO_MOCK == state)
            return __real_get_sdr_cache(pret);

        return dispatchIpmiResponse(state);
    }

    // MOCK FOR get_sdr_cache
    extern int __real_find_sdr_next(unsigned char*, unsigned  char*, unsigned short int);
    int __wrap_find_sdr_next(unsigned char *psdr, unsigned  char *pcache, unsigned short int id)
    {
        mockStates state = (mockStates) mocks[GET_SDR_CACHE].nextMockState();
        if (NO_MOCK == state)
            return __real_find_sdr_next(psdr, pcache, id);

        return -1;
    }
}

#endif // IPMIUTILAGENT_MOCKS_H

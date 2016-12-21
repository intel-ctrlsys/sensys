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

#include "testUtils.h"
#include "sensor_ipmi_sel_mocked_functions.h"

#include <map>
#include <cstdio>
#include <stdarg.h>

enum supportedMocks
{
    SET_LAN_OPTIONS,
    IPMI_CMD_MC,
    IPMI_CMD,
    GET_SDR_CACHE,
    STRDUP,
    IPMI_CMDRAW
};

enum mockStates
{
    NO_MOCK = 0,
    SUCCESS,
    FAILURE,
    RETURN_FRU_AREA_1,
    RETURN_FRU_AREA_2,
    LEGACY,
    RETRIEVE_BUFFER_1
};

extern "C"
{
    void test_error_sink_c(int level, const char* msg);
    void test_ras_event_c(const char* event, const char* hostname, void* user_object);

    extern int __real_set_lan_options(char*, char*, char*, int, int, int, void*, int);
    int __wrap_set_lan_options(char* node,
                               char* user,
                               char* pswd,
                               int auth,
                               int priv,
                               int cipher,
                               void* addr,
                               int addr_len);

    extern int __real_ipmi_cmd_mc(unsigned short, unsigned char*, int, unsigned char*, int*, unsigned char*, char);
    int __wrap_ipmi_cmd_mc(unsigned short icmd,
                           unsigned char *pdata,
                           int sdata,
                           unsigned char *presp,
                           int *sresp,
                           unsigned char *pcc,
                           char fdebugcmd);

    extern int __real_ipmi_cmd(unsigned short, unsigned char*, int, unsigned char*, int*, unsigned char*, char);
    int __wrap_ipmi_cmd(unsigned short icmd,
                        unsigned char *pdata,
                        int sdata,
                        unsigned char *presp,
                        int *sresp,
                        unsigned char *pcc,
                        char fdebugcmd);

    extern int __real_ipmi_cmdraw(unsigned char cmd,
                                  unsigned char netfn,
                                  unsigned char sa,
                                  unsigned char bus,
                                  unsigned char lun,
                                  unsigned char *pdata,
                                  int sdata,
                                  unsigned char *presp,
                                  int *sresp,
                                  unsigned char *pcc,
                                  char fdebugcmd);
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
                           char fdebugcmd);

    extern int __real_get_sdr_cache(unsigned char **pret);
    int __wrap_get_sdr_cache(unsigned char **pret);

    extern int __real_find_sdr_next(unsigned char*, unsigned  char*, unsigned short int);
    int __wrap_find_sdr_next(unsigned char *psdr, unsigned  char *pcache, unsigned short int id);

    extern int __real_rename(const char* old_name, const char* new_name);
    int __wrap_rename(const char* old_name, const char* new_name);

    extern int __real_ipmi_close();
    int __wrap_ipmi_close();

    extern void __real_opal_output_verbose(int level, int output_id, const char* format, ...);
    void __wrap_opal_output_verbose(int level, int output_id, const char* format, ...);

    extern int __real_getlogin_r(char* user, size_t user_size);
    int __wrap_getlogin_r(char* user, size_t user_size);

    extern __uid_t __real_geteuid();
    __uid_t __wrap_geteuid();

    extern char *__real_strdup(const char *s1);
    char *__wrap_strdup(const char *s1);
}

#endif // IPMIUTILAGENT_MOCKS_H

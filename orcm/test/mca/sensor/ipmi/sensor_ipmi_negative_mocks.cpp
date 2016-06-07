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

#include "sensor_ipmi_negative_mocks.h"

bool is_load_ipmi_config_file_expected_to_succeed = true;
bool is_get_bmcs_for_aggregator_expected_to_succeed = true;
bool is_opal_progress_thread_init_expected_to_succeed = true;
bool is_get_sdr_cache_expected_to_succeed = true;
int find_sdr_next_success_count = 3;

extern "C" {

    bool __wrap_load_ipmi_config_file(){
        return is_load_ipmi_config_file_expected_to_succeed;
    }

    bool __wrap_get_bmcs_for_aggregator(char* agg, ipmi_collector** bmc_list, int* n){
        if (!is_get_bmcs_for_aggregator_expected_to_succeed)
            return false;

        int i=0, size=3;
        ipmi_collector* local_bmc_list = NULL;
        local_bmc_list = (ipmi_collector*) malloc(size*sizeof(ipmi_collector));

        for (i=0; i<size; ++i) {
            strncpy(local_bmc_list[i].bmc_address, "192.168.1.1", MAX_STR_LEN-1);
            local_bmc_list[i].bmc_address[MAX_STR_LEN-1] = '\0';
            strncpy(local_bmc_list[i].user, "user", MAX_STR_LEN-1);
            local_bmc_list[i].user[MAX_STR_LEN-1] = '\0';
            strncpy(local_bmc_list[i].pass, "pass", MAX_STR_LEN-1);
            local_bmc_list[i].pass[MAX_STR_LEN-1] = '\0';
            strncpy(local_bmc_list[i].aggregator, "aggregator", MAX_STR_LEN-1);
            local_bmc_list[i].aggregator[MAX_STR_LEN-1] = '\0';
            strncpy(local_bmc_list[i].hostname, "hostname", MAX_STR_LEN-1);
            local_bmc_list[i].hostname[MAX_STR_LEN-1] = '\0';
            local_bmc_list[i].auth_method = 3;
            local_bmc_list[i].priv_level  = 3;
            local_bmc_list[i].port        = 2000;
            local_bmc_list[i].channel     = 3;
        }

        *n = i;
        *bmc_list = local_bmc_list;

        return true;
    }

    opal_event_base_t* __wrap_opal_progress_thread_init(const char *name){
        if (is_opal_progress_thread_init_expected_to_succeed)
            return __real_opal_progress_thread_init(name);
        return NULL;
    }

    int __wrap_get_sdr_cache(unsigned char **pcache){
        return is_get_sdr_cache_expected_to_succeed ? 0 : 1;
    }

    void __wrap_free_sdr_cache(unsigned char **pcache){
    }

    int __wrap_find_sdr_next(unsigned char *psdr, unsigned char *pcache, unsigned short id){
        if (!find_sdr_next_success_count)
            return 1;
        memset(psdr, 0xFF, 80);
        psdr[3] = 0x01;
        --find_sdr_next_success_count;
        return 0;
    }

    int __wrap_ipmi_cmd_mc(unsigned short icmd, unsigned char *pdata, int sdata, unsigned char *presp, int *sresp, unsigned char *pcc, char fdebugcmd){
        return 0;
    }
}

/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include <iostream>
#include <string.h>

extern "C"{
    #include "orcm/tools/octl/common.h"

    #include "orte/mca/notifier/base/base.h"

    #include "orcm/util/utils.h"
    #include "orcm/runtime/orcm_cmd_server.h"
    #include "orcm/mca/dispatch/base/base.h"
};

#include "cmd_server_tests_mocking.h"

bool is_recv_buffer_nb_called = false;
bool is_recv_cancel_called = false;
bool is_get_notifier_policy_expected_to_succeed = true;
bool is_orte_notifier_base_get_config_expected_to_succeed = true;
bool is_get_bmc_info_expected_to_succeed = true;
bool is_expected_to_succeed = true;
bool is_send_buffer_expected_to_succeed = true;
bool is_hostname_append_expected_to_succeed = true;
bool is_chassis_append_expected_to_succeed = true;
int next_state = 0x01;
int next_response = ORCM_SUCCESS;

extern "C" {
    int __wrap_orte_notifier_base_set_config(char* action, opal_value_t* kv){
        return ORCM_SUCCESS;
    }

    const char* __wrap_get_notifier_policy(orte_notifier_severity_t sev){
        if (is_get_notifier_policy_expected_to_succeed){
            return __real_get_notifier_policy(sev);
        }
        return NULL;
    }

    int __wrap_orte_notifier_base_get_config(char* action, opal_list_t** list){
        opal_value_t* kv;
        if (is_orte_notifier_base_get_config_expected_to_succeed){
            kv = orcm_util_load_opal_value((char*)"test", (void*)"test", OPAL_STRING);
            opal_list_append(*list, (opal_list_item_t*)kv);
            return ORCM_SUCCESS;
        }
        return ORCM_ERROR;
    }

    bool __wrap_load_ipmi_config_file(){
        return true;
    }

    bool __wrap_get_bmc_info(const char* hostname, ipmi_collector* ic){
        strcpy(ic->bmc_address, "192.168.1.1");
        strcpy(ic->user, "user");
        strcpy(ic->pass, "password");
        strcpy(ic->aggregator, "aggregator");
        strcpy(ic->hostname, "host01");
        ic->auth_method = 1;
        ic->priv_level  = 2;
        ic->port        = 3;
        ic->channel     = 4;
        return is_get_bmc_info_expected_to_succeed;
    }

    int __wrap_get_chassis_id_state(){
        return next_state;
    }

    int __wrap_disable_chassis_id(void){
        return is_expected_to_succeed ? ORCM_SUCCESS : ORCM_ERROR;
    }

    int __wrap_enable_chassis_id(void){
        return is_expected_to_succeed ? ORCM_SUCCESS : ORCM_ERROR;
    }

    int __wrap_enable_chassis_id_with_timeout(int){
        return is_expected_to_succeed ? ORCM_SUCCESS : ORCM_ERROR;
    }

    int __wrap_orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data, opal_data_type_t type, char *units){
        if(!is_hostname_append_expected_to_succeed && 0 == strcmp((char*)"hostname", key)){
            return ORCM_ERROR;
        }

        if(!is_chassis_append_expected_to_succeed && 0 == strcmp((char*)"chassis-id-state", key)){
            return ORCM_ERROR;
        }

        return __real_orcm_util_append_orcm_value(list, key, data, type, units);
    }
}

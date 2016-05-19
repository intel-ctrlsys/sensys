/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CMD_SERVER_TESTS_MOCKING_H
#define CMD_SERVER_TESTS_MOCKING_H

#include "orte/mca/notifier/base/base.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser_interface.h"

#ifdef __cplusplus
extern "C"{
#endif
    #include "opal/mca/event/libevent2022/libevent/include/event2/event.h"

    extern int __real_orte_notifier_base_set_config(char*, opal_value_t*);

    extern const char* __real_get_notifier_policy(orte_notifier_severity_t);

    extern int __real_orte_notifier_base_get_config(char*, opal_list_t**);

    extern bool __real_load_ipmi_config_file(void);

    extern bool __real_get_bmc_info(const char*, ipmi_collector*);

    extern int __real_get_chassis_id_state(void);

    extern int __real_disable_chassis_id(void);

    extern int __real_enable_chassis_id(void);

    extern int __real_enable_chassis_id_with_timeout(int);

    extern int __real_orcm_util_append_orcm_value(opal_list_t *, char *, void *, opal_data_type_t, char *);

#ifdef __cplusplus
}
#endif

extern bool is_recv_buffer_nb_called;
extern bool is_recv_cancel_called;
extern bool is_get_notifier_policy_expected_to_succeed;
extern bool is_orte_notifier_base_get_config_expected_to_succeed;
extern bool is_get_bmc_info_expected_to_succeed;
extern bool is_expected_to_succeed;
extern bool is_send_buffer_expected_to_succeed;
extern bool is_hostname_append_expected_to_succeed;
extern bool is_chassis_append_expected_to_succeed;
extern int next_state;
extern int next_response;

#endif // CMD_SERVER_TESTS_MOCKING_H

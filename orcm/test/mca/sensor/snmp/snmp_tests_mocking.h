/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_TESTS_MOCKING_H
#define SNMP_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <sys/stat.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include "orte/include/orte/types.h"
    #include "orcm/mca/analytics/analytics_types.h"
    #include <net-snmp/net-snmp-config.h>
    #include <net-snmp/net-snmp-includes.h>
    #include <net-snmp/library/transform_oids.h>
    #include <net-snmp/mib_api.h>

    extern void __real_orte_errmgr_base_log(int err, char* file, int lineno);
    extern void __real_opal_output_verbose(int level, int output_id, const char* format, ...);
    extern char* __real_orte_util_print_name_args(const orte_process_name_t* name);
    extern void __real_orcm_analytics_base_send_data(orcm_analytics_value_t *data);
    extern opal_event_base_t* __real_opal_progress_thread_init(const char* name);
    extern int __real_opal_progress_thread_finalize(const char* name);
    extern struct snmp_session* __real_snmp_open(struct snmp_session* ss);
    extern int __real_snmp_synch_response(netsnmp_session *session, netsnmp_pdu *pdu, netsnmp_pdu **response);
    extern void __real_snmp_free_pdu(netsnmp_pdu *pdu);
    extern struct snmp_pdu* __real_snmp_pdu_create(int command);
    extern int __real_snmp_parse_oid(const char *input, oid *objid, size_t *objidlen);
    extern int __real_snprint_objid(char *buf, size_t len, oid *objid, size_t *objidlen);
    extern char* __real_orcm_get_proc_hostname(void);
    extern netsnmp_variable_list* __real_snmp_add_null_var(netsnmp_pdu *pdu, const oid *objid, size_t objidlen);
    extern orcm_analytics_value_t* __real_orcm_util_load_orcm_analytics_value(opal_list_t *key,
                                          opal_list_t *non_compute, opal_list_t *compute);

#ifdef __cplusplus
}
#endif // __cplusplus

typedef void (*orte_errmgr_base_log_callback_fn_t)(int err, char* file, int lineno);
typedef void (*opal_output_verbose_callback_fn_t)(int level, int id, const char* line);
typedef char* (*orte_util_print_name_args_fn_t)(const orte_process_name_t* name);
typedef void (*orcm_analytics_base_send_data_fn_t)(orcm_analytics_value_t *data);
typedef opal_event_base_t* (*opal_progress_thread_init_fn_t)(const char* name);
typedef int (*opal_progress_thread_finalize_fn_t)(const char* name);
typedef struct snmp_session* (*snmp_open_callback_fn_t)(struct snmp_session* ss);
typedef int (*snmp_synch_response_callback_ft_t)(netsnmp_session *session, netsnmp_pdu *pdu, netsnmp_pdu **response);
typedef void (*snmp_free_pdu_callback_ft_t)(netsnmp_pdu* pdu);
typedef struct snmp_pdu* (*snmp_pdu_create_callback_ft_t)(int command);
typedef oid* (*snmp_parse_oid_callback_ft_t)(const char *input, oid *objid, size_t *objidlen);
typedef int (*snprint_objid_callback_ft_t)(char *buf, size_t len, oid *objid, size_t *objidlen);
typedef netsnmp_variable_list* (*snmp_add_null_var_callback_ft_t)(netsnmp_pdu *pdu, const oid *objid, size_t objidlen);
typedef orcm_analytics_value_t* (*orcm_util_load_orcm_analytics_value_fn_t)(opal_list_t *key,
                                          opal_list_t *non_compute, opal_list_t *compute);


class snmp_tests_mocking
{
    public:
        // Construction
        snmp_tests_mocking();

        // Public Callbacks
        orte_errmgr_base_log_callback_fn_t orte_errmgr_base_log_callback;
        opal_output_verbose_callback_fn_t opal_output_verbose_callback;
        orte_util_print_name_args_fn_t orte_util_print_name_args_callback;
        orcm_analytics_base_send_data_fn_t orcm_analytics_base_send_data_callback;
        opal_progress_thread_init_fn_t opal_progress_thread_init_callback;
        opal_progress_thread_finalize_fn_t opal_progress_thread_finalize_callback;
        snmp_open_callback_fn_t snmp_open_callback;
        snmp_synch_response_callback_ft_t snmp_synch_response_callback;
        snmp_free_pdu_callback_ft_t snmp_free_pdu_callback;
        snmp_pdu_create_callback_ft_t snmp_pdu_create_callback;
        snmp_parse_oid_callback_ft_t snmp_parse_oid_callback;
        snmp_add_null_var_callback_ft_t snmp_add_null_var_callback;
        snprint_objid_callback_ft_t snprint_objid_callback;
        orcm_util_load_orcm_analytics_value_fn_t orcm_util_load_orcm_analytics_value_callback;
};

extern snmp_tests_mocking snmp_mocking;

#endif // SNMP_TESTS_MOCKING_H

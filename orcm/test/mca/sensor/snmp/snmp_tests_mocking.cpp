/*
 * Copyright (c) 2015-2016  Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_tests_mocking.h"

#include <iostream>
using namespace std;

snmp_tests_mocking snmp_mocking;

extern "C" { // Mocking must use correct "C" linkages
    #include <stdarg.h>

    void __wrap_snmp_open(struct snmp_session *ss)
    {
        if(NULL == snmp_mocking.snmp_open_callback) {
            __real_snmp_open(ss);
        } else {
            snmp_mocking.snmp_open_callback(ss);
        }
    }

    void __wrap_snmp_synch_response(netsnmp_session *session, netsnmp_pdu *pdu,
                                    netsnmp_pdu **response)
    {
        if(NULL == snmp_mocking.snmp_synch_response_callback) {
              __real_snmp_synch_response(session, pdu, response);
        } else {
             snmp_mocking.snmp_synch_response_callback(session, pdu, response);
        }
    }

    void __wrap_snmp_free_pdu(netsnmp_pdu *pdu)
    {
        if(NULL == snmp_mocking.snmp_free_pdu_callback) {
            __real_snmp_free_pdu(pdu);
        } else {
            snmp_mocking.snmp_free_pdu_callback(pdu);
        }
    }

    void __wrap_snmp_pdu_create(int command)
    {
        if(NULL == snmp_mocking.snmp_pdu_create_callback) {
            __real_snmp_pdu_create(command);
        } else {
            snmp_mocking.snmp_pdu_create_callback(command);
        }
    }

    void __wrap_snmp_parse_oid(const char *input, oid *objid, size_t *objidlen)
    {
        if(NULL == snmp_mocking.snmp_parse_oid_callback) {
            __real_snmp_parse_oid(input, objid, objidlen);
        } else {
            snmp_mocking.snmp_parse_oid_callback(input, objid, objidlen);
        }
    }

    void __wrap_snprint_objid(char *buf, size_t len, oid *objid, size_t *objidlen)
    {
        if(NULL == snmp_mocking.snprint_objid_callback) {
          __real_snprint_objid(buf, len, objid, objidlen);
        } else {
          snmp_mocking.snprint_objid_callback(buf, len, objid, objidlen);
        }
    }

    void __wrap_snmp_add_null_var(netsnmp_pdu *pdu, const oid *objid, size_t objidlen)
    {
        if(NULL == snmp_mocking.snmp_add_null_var_callback) {
            __real_snmp_add_null_var(pdu, objid, objidlen);
        } else {
            snmp_mocking.snmp_add_null_var_callback(pdu, objid, objidlen);
        }
    }

    void __wrap_orte_errmgr_base_log(int err, char* file, int lineno)
    {
        if(NULL == snmp_mocking.orte_errmgr_base_log_callback) {
            __real_orte_errmgr_base_log(err, file, lineno);
        } else {
            snmp_mocking.orte_errmgr_base_log_callback(err, file, lineno);
        }
    }

    void __wrap_opal_output_verbose(int level, int output_id, const char* format, ...)
    {
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
        if(NULL == snmp_mocking.opal_output_verbose_callback) {
            __real_opal_output_verbose(level, output_id, (const char*)buffer);
        } else {
            snmp_mocking.opal_output_verbose_callback(level, output_id, (const char*)buffer);
        }
    }

    char* __wrap_orte_util_print_name_args(const orte_process_name_t* name)
    {
        if(NULL == snmp_mocking.orte_util_print_name_args_callback) {
            __real_orte_util_print_name_args(name);
        } else {
            snmp_mocking.orte_util_print_name_args_callback(name);
        }
    }

    orcm_analytics_value_t* __wrap_orcm_util_load_orcm_analytics_value(opal_list_t *key,
                                   opal_list_t *non_compute,  opal_list_t *compute)
    {
        if(NULL == snmp_mocking.orcm_util_load_orcm_analytics_value_callback) {
            __real_orcm_util_load_orcm_analytics_value(key, non_compute, compute);
        } else {
            snmp_mocking.orcm_util_load_orcm_analytics_value_callback(key, non_compute, compute);
        }
    }

    void __wrap_orcm_analytics_base_send_data(orcm_analytics_value_t *data)
    {
        if(NULL == snmp_mocking.orcm_analytics_base_send_data_callback) {
            __real_orcm_analytics_base_send_data(data);
        } else {
            snmp_mocking.orcm_analytics_base_send_data_callback(data);
        }
    }
    opal_event_base_t* __wrap_opal_progress_thread_init(const char* name)
    {
        if(NULL == snmp_mocking.opal_progress_thread_init_callback) {
            __real_opal_progress_thread_init(name);
        } else {
            snmp_mocking.opal_progress_thread_init_callback(name);
        }
    }
    int __wrap_opal_progress_thread_finalize(const char* name)
    {
        if(NULL == snmp_mocking.opal_progress_thread_finalize_callback) {
            __real_opal_progress_thread_finalize(name);
        } else {
            snmp_mocking.opal_progress_thread_finalize_callback(name);
        }
    }

    char* __wrap_orcm_get_proc_hostname(void)
    {
       return (char*) "localhost";
    }
} // extern "C"

snmp_tests_mocking::snmp_tests_mocking() :
    orte_errmgr_base_log_callback(NULL), opal_output_verbose_callback(NULL),
    orte_util_print_name_args_callback(NULL), orcm_analytics_base_send_data_callback(NULL),
    opal_progress_thread_init_callback(NULL), opal_progress_thread_finalize_callback(NULL),
    snmp_open_callback(NULL), snmp_synch_response_callback(NULL), snmp_free_pdu_callback(NULL),
    snmp_pdu_create_callback(NULL), snmp_parse_oid_callback(NULL), snmp_add_null_var_callback(NULL),
    snprint_objid_callback(NULL), orcm_util_load_orcm_analytics_value_callback(NULL)
{
}

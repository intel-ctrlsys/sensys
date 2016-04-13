/*
 * Copyright (c) 2016      Intel Inc
 *                         All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <string.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "orcm_config.h"
#include "orcm/constants.h"
#include "opal/dss/dss.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_wait.h"
#include "orte/mca/notifier/base/base.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/runtime/orcm_cmd_server.h"
#include "orcm/runtime/led_control_interface.h"

#include "orcm/util/utils.h"
#include "orcm/util/logical_group.h"
#include "orcm/mca/dispatch/base/base.h"
#include "orcm/mca/dispatch/dispatch.h"
#include "orcm/mca/sensor/ipmi/ipmi_parser_interface.h"

#define  NULL_CHECK(p) if(NULL==p) {goto ERROR;}
#define SAFE_RELEASE(p) if(NULL != p) OBJ_RELEASE(p);
#define SAFE_ARGV_FREE(p) if(NULL != p) {opal_argv_free(p);p=NULL;}

static bool recv_issued=false;
static int unpack_command_subcommand(opal_buffer_t* buffer, orcm_cmd_server_flag_t *command,
                                     orcm_cmd_server_flag_t *sub_command);
static int orcm_cmd_server_set_policy(opal_buffer_t* buffer);
static int orcm_cmd_server_get_policy(opal_buffer_t* buffer, opal_buffer_t **pack_buffer, int *count);
static int pack_severity_action(opal_buffer_t **result_buff, orte_notifier_severity_t *sev,
                                char **action);
static int unpack_severity_action(opal_buffer_t *buffer, orte_notifier_severity_t *sev,
                                char **action);
static int orcm_cmd_server_set_smtp(opal_buffer_t* buffer);
static int pack_opal_value(opal_buffer_t **buffer, opal_value_t **kv);
static int unpack_opal_value(opal_buffer_t *buffer, opal_value_t **kv);
static int orcm_cmd_server_get_smtp(opal_buffer_t* buffer, opal_buffer_t **result_buff, int *count);
static void store_chassis_id_event(char* hostname, char *action);
static int append_string_to_opal_list(opal_list_t *list, char *str, char* key);
static void chassis_id_event_cleanup(void *cbdata);
static int chassis_id_operation(orcm_cmd_server_flag_t sub_command,
        char* hostname, char* bmc_addr, char* user, char* pass,
        int auth, int priv, unsigned int seconds, opal_buffer_t *pack_buff);
static int get_visible_nodes_count(char** nodelist, int node_count);

int orcm_cmd_server_init(void)
{
    if (!recv_issued) {
        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                ORCM_RML_TAG_CMD_SERVER,
                                ORTE_RML_PERSISTENT,
                                orcm_cmd_server_recv,
                                NULL);
        recv_issued = true;
    }
    return ORCM_SUCCESS;
}

void orcm_cmd_server_finalize(void)
{
    if (recv_issued) {
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
        recv_issued = false;
    }
}

void orcm_cmd_server_recv(int status, orte_process_name_t* sender,
                      opal_buffer_t* buffer, orte_rml_tag_t tag,
                      void* cbdata)
{
    orcm_cmd_server_flag_t command, sub_command;
    opal_buffer_t *pack_buff = NULL;
    opal_buffer_t *result_buff = NULL;
    char *error = NULL;
    int rc = ORCM_SUCCESS;
    int rc_prev = ORCM_SUCCESS;
    int response = ORCM_SUCCESS;
    int count = 0;
    int cnt = 1;
    int seconds = 0;
    char* noderaw = NULL;
    int node_count = 0;
    char **nodelist = NULL;
    int iter = 0;
    ipmi_collector ipmi_c;
    int visible_nodes = 0;

    rc = unpack_command_subcommand(buffer, &command, &sub_command);
    if (ORCM_SUCCESS != rc) {
        goto ERROR;
    }
    if (ORCM_SET_NOTIFIER_COMMAND == command) {
        pack_buff = OBJ_NEW(opal_buffer_t);
        NULL_CHECK(pack_buff);
        switch(sub_command) {
        case ORCM_SET_NOTIFIER_POLICY_COMMAND:
            response = orcm_cmd_server_set_policy(buffer);
            break;
        case ORCM_SET_NOTIFIER_SMTP_COMMAND:
            response = orcm_cmd_server_set_smtp(buffer);
            break;
        default:
            asprintf(&error,"invalid notifier set command");
            goto ERROR;
        }
        if (ORCM_SUCCESS != response) {
            goto ERROR;
        }
        if (OPAL_SUCCESS != 
            (rc = opal_dss.pack(pack_buff, &response, 1, OPAL_INT))) {
             goto ERROR;
        }
        goto RESPONSE;

    } else if (ORCM_GET_NOTIFIER_COMMAND == command) {
        pack_buff = OBJ_NEW(opal_buffer_t);
        NULL_CHECK(pack_buff);
        switch(sub_command) {
        case ORCM_GET_NOTIFIER_POLICY_COMMAND:
            response = orcm_cmd_server_get_policy(buffer, &result_buff, &count);
            break;
        case ORCM_GET_NOTIFIER_SMTP_COMMAND:
            response = orcm_cmd_server_get_smtp(buffer, &result_buff, &count);
            break;
        default:
            asprintf(&error,"invalid notifier get command");
            goto ERROR;
        }
        if (ORCM_SUCCESS != response) {
            goto ERROR;
        }
        rc = opal_dss.pack(pack_buff, &response, 1, OPAL_INT);
        if (OPAL_SUCCESS != rc) {
            goto ERROR;
        }
        rc = opal_dss.pack(pack_buff, &count, 1, OPAL_INT);
        if (OPAL_SUCCESS != rc) {
            goto ERROR;
        }
        rc = opal_dss.pack(pack_buff, &result_buff, 1, OPAL_BUFFER);
        goto RESPONSE;
    } else if (ORCM_GET_CHASSIS_ID == command || ORCM_SET_CHASSIS_ID == command){
        pack_buff = OBJ_NEW(opal_buffer_t);
        NULL_CHECK(pack_buff);

        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &noderaw,
                                                 &cnt, OPAL_STRING))){
            goto ERROR;
        }

        cnt = 1;
        seconds = 0;
        if (ORCM_SET_CHASSIS_ID_TEMPORARY_ON == sub_command &&
            OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &seconds, &cnt, OPAL_INT))){
            goto ERROR;
        }

        orcm_logical_group_parse_array_string(noderaw, &nodelist);
        node_count = opal_argv_count(nodelist);
        if (!node_count){
            asprintf(&error, "nodelist not found");
            goto ERROR;
        }

        (void) load_ipmi_config_file();
        rc = ORCM_SUCCESS;
        rc_prev = ORCM_SUCCESS;
        visible_nodes = get_visible_nodes_count(nodelist, node_count);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &visible_nodes, 1, OPAL_INT))){
            goto ERROR;
        }

        for (iter = 0; iter < node_count; iter++){
            if (!get_bmc_info(nodelist[iter], &ipmi_c)){
                continue;
            }
            if (!strcmp(orte_process_info.nodename, ipmi_c.aggregator)){
                rc = chassis_id_operation(sub_command, ipmi_c.hostname,
                        ipmi_c.bmc_address, ipmi_c.user, ipmi_c.pass,
                        ipmi_c.auth_method, ipmi_c.priv_level, seconds,
                        pack_buff);
                rc_prev = (ORCM_SUCCESS != rc_prev) ? rc_prev : rc;
            }
        }
        goto RESPONSE;
    }

ERROR:
    SAFE_ARGV_FREE(nodelist);
    if (NULL == pack_buff) {
         pack_buff = OBJ_NEW(opal_buffer_t);
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &response, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        SAFE_RELEASE(pack_buff);
        if (NULL != error) {
            free(error);
        }
        return;
    }
    if (NULL == error) {
        asprintf(&error,"notifier data buffer mismatch");
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &error, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        SAFE_RELEASE(pack_buff);
        free(error);
        return;
    }


RESPONSE:
    SAFE_ARGV_FREE(nodelist);
    if (NULL != error) {
        free(error);
    }
    if (ORTE_SUCCESS !=
        (rc = orte_rml.send_buffer_nb(sender, pack_buff,
                                      ORCM_RML_TAG_CMD_SERVER,
                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        SAFE_RELEASE(pack_buff);
        return;
    }
}

static int get_visible_nodes_count(char** nodelist, int node_count){
    int count=0;
    int iter=0;
    ipmi_collector ipmi_c;

    for (iter = 0; iter < node_count; iter++){
        if (!get_bmc_info(nodelist[iter], &ipmi_c)){
            continue;
        }
        if (!strcmp(orte_process_info.nodename, ipmi_c.aggregator)){
            ++count;
        }
    }
    return count;
}

static int unpack_command_subcommand(opal_buffer_t* buffer, orcm_cmd_server_flag_t *command,
                                     orcm_cmd_server_flag_t *sub_command)
{
    int rc = ORCM_SUCCESS;
    int cnt = 1;
    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, command,
                                              &cnt, ORCM_CMD_SERVER_T))) {
        return rc;
    }
    cnt = 1;
    /* unpack the subcommand */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, sub_command,
                                              &cnt, ORCM_CMD_SERVER_T))) {
        return rc;
    }
    return rc;
}

static int orcm_cmd_server_set_policy(opal_buffer_t* buffer)
{
    int rc = ORCM_SUCCESS;
    orte_notifier_severity_t sev;
    char *action = NULL;

    rc = unpack_severity_action(buffer, &sev, &action);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }
    return set_notifier_policy(sev, action);
}

static int orcm_cmd_server_get_policy(opal_buffer_t* buffer, opal_buffer_t **result_buff, int *count)
{
    int rc = ORCM_SUCCESS;
    orte_notifier_severity_t sev;
    char *action = NULL;
    int pack_count = 0;

    *result_buff = OBJ_NEW(opal_buffer_t);
    if(NULL == *result_buff) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    for(sev = ORTE_NOTIFIER_EMERG; sev <= ORTE_NOTIFIER_DEBUG; sev++) {
        action = (char *) get_notifier_policy(sev);
        pack_count++;
        rc = pack_severity_action(result_buff, &sev, &action);
        if (ORCM_SUCCESS != rc) {
            SAFE_RELEASE(*result_buff);
            *result_buff = NULL;
            return rc;
        }
    }
    *count = pack_count;
    return rc;
}

static int pack_severity_action(opal_buffer_t **buffer, orte_notifier_severity_t *sev,
                                char **action)
{
    int rc = ORCM_SUCCESS;

    if (NULL == *action) {
        *action = orte_notifier_base.default_actions;
    }
    rc = opal_dss.pack(*buffer, sev, 1, ORTE_NOTIFIER_SEVERITY_T);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }
    rc = opal_dss.pack(*buffer, action, 1, OPAL_STRING);
    return rc;
}

static int unpack_severity_action(opal_buffer_t *buffer, orte_notifier_severity_t *sev,
                                char **action)
{
    int rc = ORCM_SUCCESS;
    int cnt = 1;
        /* unpack the severity */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, sev,
                                              &cnt, ORTE_NOTIFIER_SEVERITY_T))) {
        return rc;
    }
    cnt = 1;
    /* unpack the action */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, action,
                                              &cnt, OPAL_STRING))) {
        return rc;
    }
    return rc;
}

static int orcm_cmd_server_set_smtp(opal_buffer_t* buffer)
{
    int rc = ORCM_SUCCESS;
    opal_value_t *kv = NULL;

    rc = unpack_opal_value(buffer, &kv);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }
    return orte_notifier_base_set_config("smtp",kv);
}

static int pack_opal_value(opal_buffer_t **buffer, opal_value_t **kv)
{
    return opal_dss.pack(*buffer, kv, 1, OPAL_VALUE);
}

static int unpack_opal_value(opal_buffer_t *buffer, opal_value_t **kv)
{
    int cnt = 1;
    /*unpack the opal_value_t*/
    return opal_dss.unpack(buffer, kv,
                           &cnt, OPAL_VALUE);
}

static int orcm_cmd_server_get_smtp(opal_buffer_t* buffer, opal_buffer_t **result_buff, int *count)
{
    int rc = ORCM_SUCCESS;
    opal_list_t *list = NULL;
    opal_value_t *kv = NULL;
    int pack_count = 0;

    *result_buff = OBJ_NEW(opal_buffer_t);
    if(NULL == *result_buff) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    list = OBJ_NEW(opal_list_t);
    if(NULL == list) {
        OBJ_RELEASE(*result_buff);
        *result_buff = NULL;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    rc = orte_notifier_base_get_config("smtp", &list);
    if (ORCM_SUCCESS != rc) {
        OBJ_RELEASE(*result_buff);
        OBJ_RELEASE(list);
        *result_buff = NULL;
        return rc;
    }
    OPAL_LIST_FOREACH(kv, list, opal_value_t) {
        pack_count++;
        rc = pack_opal_value(result_buff, &kv);
        if (ORCM_SUCCESS != rc) {
            OBJ_RELEASE(*result_buff);
            OBJ_RELEASE(list);
            *result_buff = NULL;
            return rc;
        }
    }
    *count = pack_count;
    OBJ_RELEASE(list);
    return rc;
}

static void store_chassis_id_event(char* hostname, char *action){
    orcm_ras_event_t* event_data = OBJ_NEW(orcm_ras_event_t);
    struct timeval timestamp;
    int rc = ORCM_SUCCESS;

    if (NULL == event_data){
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return;
    }
    rc = append_string_to_opal_list(&(event_data->reporter), hostname, "hostname");
    if (ORCM_SUCCESS != rc){
        SAFE_RELEASE(event_data);
        ORTE_ERROR_LOG(rc);
        return;
    }

    rc = append_string_to_opal_list(&(event_data->data), action, "chassis-id-state");
    if (ORCM_SUCCESS != rc){
        SAFE_RELEASE(event_data);
        ORTE_ERROR_LOG(rc);
        return;
    }

    gettimeofday(&timestamp, NULL);
    event_data->timestamp = timestamp.tv_sec;
    event_data->type = ORCM_RAS_EVENT_CHASSIS_ID_LED;
    event_data->severity = ORCM_RAS_SEVERITY_INFO;
    event_data->cbfunc = chassis_id_event_cleanup;
    ORCM_RAS_EVENT(event_data);
}

static int append_string_to_opal_list(opal_list_t *list, char* str, char* key){
    return orcm_util_append_orcm_value(list, key, str, OPAL_STRING, NULL);
}

static void chassis_id_event_cleanup(void *cbdata){
    orcm_ras_event_t *event = (orcm_ras_event_t*)cbdata;
    SAFE_RELEASE(event);
}

static int pack_response(opal_buffer_t *pack_buff, int response, int state,
        char* node, orcm_cmd_server_flag_t sub_command){
    int rc = ORCM_SUCCESS;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &node, 1, OPAL_STRING))){
        return rc;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &response, 1, OPAL_INT))){
        return rc;
    }

    if (ORCM_GET_CHASSIS_ID_STATE == sub_command){
        if (OPAL_SUCCESS != (rc = opal_dss.pack(pack_buff, &state, 1, OPAL_INT))){
            return rc;
        }
    }
    return rc;
}

static int chassis_id_operation(orcm_cmd_server_flag_t sub_command,
        char* hostname, char* bmc_addr, char* user, char* pass,
        int auth, int priv, unsigned int seconds, opal_buffer_t *pack_buff){
    int state = 0;
    int response = ORCM_SUCCESS;
    int rc = ORCM_SUCCESS;

    init_led_control(bmc_addr, user, pass, auth, priv);
    switch (sub_command){
        case ORCM_GET_CHASSIS_ID_STATE:
            response = ORCM_ERROR;
            state = get_chassis_id_state();
            if (0 <= state){
                response = ORCM_SUCCESS;
            }
            break;
        case ORCM_SET_CHASSIS_ID_OFF:
            response = ORCM_ERROR;
            if (!disable_chassis_id()){
                response = ORCM_SUCCESS;
                store_chassis_id_event(hostname, "OFF");
            }
            break;
        case ORCM_SET_CHASSIS_ID_ON:
            response = ORCM_ERROR;
            if (!enable_chassis_id()){
                response = ORCM_SUCCESS;
                store_chassis_id_event(hostname, "ON");
            }
            break;
        case ORCM_SET_CHASSIS_ID_TEMPORARY_ON:
            response = ORCM_ERROR;
            if (!enable_chassis_id_with_timeout(seconds)){
                response = ORCM_SUCCESS;
                store_chassis_id_event(hostname, "TEMPORARY_ON");
            }
            break;
        default:
            fini_led_control();
            return ORCM_ERROR;
    }

    rc = pack_response(pack_buff, response, state, hostname, sub_command);
    fini_led_control();
    return rc;
}

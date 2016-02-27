/*
 * Copyright (c) 2016  Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte/mca/notifier/notifier.h"
#include "orcm/util/logical_group.h"
#include "orcm/tools/octl/common.h"

#define MIN_CHASSIS_ID_ARG 3
#define MAX_CHASSIS_ID_ARG 4

#define LED_OFF 0
#define LED_TEMPORARY_ON 1
#define LED_INDEFINITE_ON 2

static void cleanup(char **nodelist, opal_buffer_t *buf, orte_rml_recv_cb_t *xfer){
    if (nodelist)
        opal_argv_free(nodelist);

    if (buf)
        OBJ_RELEASE(buf);

    if (xfer)
        OBJ_RELEASE(xfer);

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
}

static void show_help(const char* tag, const char* error, int rc){
    if (ORCM_SUCCESS == rc)
        return;
    orte_show_help("help-octl.txt",
                   tag, true, error, ORTE_ERROR_NAME(rc), rc);
}

static void show_header(void){
    printf("\n\nNode          Chassis ID LED\n");
    printf("-----------------------------------\n");
}

static int begin_transaction(char* node, opal_buffer_t *buf, orte_rml_recv_cb_t *xfer){
    int rc = ORCM_SUCCESS;
    OBJ_RETAIN(buf);
    OBJ_RETAIN(xfer);

    xfer->active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_CMD_SERVER,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, xfer);

    orte_process_name_t target;
    rc = orcm_cfgi_base_get_hostname_proc(node, &target);
    if (ORCM_SUCCESS != rc){
        printf("\nError: node %s\n", node);
        return rc;
    }

    rc = orte_rml.send_buffer_nb(&target, buf, ORCM_RML_TAG_CMD_SERVER,
                                 orte_rml_send_callback, NULL);
    if (ORTE_SUCCESS != rc){
        printf("\nError: node %s\n", node);
        return rc;
    }

    return rc;
}

static int unpack_response(char* node, orte_rml_recv_cb_t *xfer){
    int rc = ORCM_SUCCESS;
    int response = ORCM_SUCCESS;
    int elements = 1;

    rc = opal_dss.unpack(&xfer->data, &response, &elements, OPAL_INT);
    if (OPAL_SUCCESS != rc){
        printf("\nError: node %s\n", node);
        return rc;
    }

    if (ORCM_SUCCESS != response){
        printf("\nNode %s failed to get chassis id status\n", node);
        return response;
    }

    return rc;
}

static int show_state(char* node, orte_rml_recv_cb_t *xfer){

    int elements = 1;
    int state = -1;
    int rc = opal_dss.unpack(&xfer->data, &state, &elements, OPAL_INT);
    if (OPAL_SUCCESS != rc){
        printf("\nError: node %s\n", node);
        return rc;
    }

    switch (state){
        case LED_OFF:
            printf("%-10s        %10s \n", node, "OFF");
            break;
        case LED_INDEFINITE_ON:
            printf("%-10s        %10s \n", node, "ON");
            break;
        case LED_TEMPORARY_ON:
            printf("%-10s        %10s \n", node, "TEMPORARY_ON");
            break;
        default:
            printf("%-10s        %10s \n", node, "ERROR");
            break;
    }

    return rc;
}

int orcm_octl_led_operation(orcm_cmd_server_flag_t command, orcm_cmd_server_flag_t subcommand, char *noderaw, int seconds){
    char **nodelist = NULL;
    orte_rml_recv_cb_t *xfer = NULL;
    opal_buffer_t *buf = NULL;
    int rc = ORCM_SUCCESS;
    char error[255];

    // TODO: Change this to get the aggregator!!!
    orcm_logical_group_parse_array_string(noderaw, &nodelist);
    int node_count = opal_argv_count(nodelist);
    if (0 == node_count){
        opal_argv_free(nodelist);
        nodelist = NULL;
        sprintf(error, "nodelist not found");
        rc = ORCM_ERR_BAD_PARAM;
        cleanup(nodelist, buf, xfer);
        show_help(TAG, error, rc);
        return rc;
    }
    ///////

    buf = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command, 1, ORCM_CMD_SERVER_T))){
        sprintf(error, PACKERR);
        cleanup(nodelist, buf, xfer);
        show_help(TAG, error, rc);
        return rc;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &subcommand, 1, ORCM_CMD_SERVER_T))){
        sprintf(error, PACKERR);
        cleanup(nodelist, buf, xfer);
        show_help(TAG, error, rc);
        return rc;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &seconds, 1, OPAL_INT))){
        sprintf(error, PACKERR);
        cleanup(nodelist, buf, xfer);
        show_help(TAG, error, rc);
        return rc;
    }

    xfer = OBJ_NEW(orte_rml_recv_cb_t);
    if (ORCM_GET_CHASSIS_ID == command)
        show_header();
    int i=0;
    for (i = 0; i < node_count; ++i){
        if (ORCM_SUCCESS != begin_transaction(nodelist[i], buf, xfer)){
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
            continue;
        }

        ORTE_WAIT_FOR_COMPLETION(xfer->active);

        if (ORCM_SUCCESS != unpack_response(nodelist[i], xfer)){
            orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_CMD_SERVER);
            continue;
        }

        if (ORCM_GET_CHASSIS_ID == command)
            show_state(nodelist[i], xfer);
        else
            printf("\n");
    }

    cleanup(nodelist, buf, xfer);
    show_help(TAG, error, rc);
    return rc;
}

int orcm_octl_chassis_id_state(char **argv){
    int rc = ORCM_SUCCESS;
    if (MIN_CHASSIS_ID_ARG != opal_argv_count(argv)){
        rc = ORCM_ERR_BAD_PARAM;
        show_help("octl:chassis-id:state-usage", "incorrect arguments!", rc);
        return rc;
    }

    return orcm_octl_led_operation(ORCM_GET_CHASSIS_ID, ORCM_GET_CHASSIS_ID_STATE, argv[2], 0);
}

int orcm_octl_chassis_id_off(char **argv){
    int rc = ORCM_SUCCESS;
    if (MIN_CHASSIS_ID_ARG != opal_argv_count(argv)){
        rc = ORCM_ERR_BAD_PARAM;
        show_help("octl:chassis-id:disable-usage", "incorrect arguments!", rc);
        return rc;
    }
    return orcm_octl_led_operation(ORCM_SET_CHASSIS_ID, ORCM_SET_CHASSIS_ID_OFF, argv[2], 0);
}

int orcm_octl_chassis_id_on(char **argv){
    int rc = ORCM_SUCCESS;
    int argv_count = opal_argv_count(argv);
    if (MIN_CHASSIS_ID_ARG == argv_count)
        rc = orcm_octl_led_operation(ORCM_SET_CHASSIS_ID, ORCM_SET_CHASSIS_ID_ON, argv[2], 0);
    else if (MAX_CHASSIS_ID_ARG == argv_count)
        rc = orcm_octl_led_operation(ORCM_SET_CHASSIS_ID, ORCM_SET_CHASSIS_ID_TEMPORARY_ON, argv[3], atoi(argv[2]));
    else {
        rc = ORCM_ERR_BAD_PARAM;
        show_help("octl:chassis-id:enable-usage", "incorrect arguments!", rc);
    }
    return rc;
}

/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include <math.h>

#include "orcm/tools/octl/common.h"
#include "orcm/util/attr.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/pwrmgmt/pwrmgmt.h"

int orcm_octl_power_set(int cmd, char **argv)
{
    orcm_scd_cmd_flag_t command;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t xfer;
    int rc, n, result;
    int32_t int_param;
    float float_param;
    bool bool_param;
    bool per_session = false;
    
    if (4 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"power set\"\n");
        return ORCM_ERR_BAD_PARAM;
    }

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);
    
    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    
    command = ORCM_SET_POWER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    
    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* This is a global power setting, not per session */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &per_session,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    switch(cmd) {
    case ORCM_SET_POWER_BUDGET_COMMAND:
         int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power budget is reasonable
            
        /* pack the power budget */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_MODE_COMMAND:
        if (isdigit(argv[3][strlen(argv[3]) - 1])) {
            int_param = (int)strtol(argv[3], NULL, 10);
        }
        else {
            int_param = orcm_pwrmgmt_get_mode_val(argv[3]); 
        }
        
        if (0 > int_param || ORCM_PWRMGMT_NUM_MODES <= int_param ) {
            printf("Illegal Power Management Mode\n");
            return ORCM_ERR_BAD_PARAM;
        }
        // FIXME: validate that power mode is valid
            
        /* pack the power mode */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_WINDOW_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power window is reasonable
            
        /* pack the power window */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_OVERAGE_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power overage is reasonable
            
        /* pack the power overage */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_UNDERAGE_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power underage is reasonable
            
        /* pack the power underage */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_OVERAGE_TIME_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power overage time is reasonable
            
        /* pack the power overage time */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_UNDERAGE_TIME_COMMAND:
        int_param = (int)strtol(argv[3], NULL, 10);
        // FIXME: validate that power underage time is reasonable
            
        /* pack the power underage time */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &int_param, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_FREQUENCY_COMMAND:
        if (isdigit(argv[3][strlen(argv[3]) - 1])) {
            float_param = (float)strtof(argv[3], NULL);
        }
        else {
            if(!strncmp(argv[3], "MAX_FREQ", 8)) {
                float_param = ORCM_PWRMGMT_MAX_FREQ;
            }
            else if(!strncmp(argv[3], "MIN_FREQ", 8)) {
                float_param = ORCM_PWRMGMT_MIN_FREQ;
            }
            else {
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                OBJ_RELEASE(buf);
                OBJ_DESTRUCT(&xfer);
                return ORCM_ERR_BAD_PARAM; 
            }
        }
        // FIXME: validate that power freq is reasonable
            
        /* pack the power freq */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &float_param, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    case ORCM_SET_POWER_STRICT_COMMAND:
        if (isdigit(argv[3][strlen(argv[3]) - 1])) {
            bool_param = (bool)strtol(argv[3], NULL, 10);
        }
        else {
            if(!strncmp(argv[3], "true", 4)) {
                bool_param = true;
            }
            else if(!strncmp(argv[3], "false", 5)) {
                bool_param = false;
            }
            else {
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                OBJ_RELEASE(buf);
                OBJ_DESTRUCT(&xfer);
                return ORCM_ERR_BAD_PARAM; 
            }
        }
        // FIXME: validate that power freq is reasonable
            
        /* pack the power freq */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &bool_param, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(buf);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        break;
    default:
        printf("Illegal power command: %d\n", cmd);
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return ORTE_ERR_BAD_PARAM;
    }

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                      buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    /* get result */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &result,
                                              &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    if (0 == result) {
        printf("Success\n");
    } else {
        printf("Failure\n");
    }
    
    return ORCM_SUCCESS;
}

int orcm_octl_power_get(int cmd, char **argv)
{
    orcm_scd_cmd_flag_t command;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t xfer;
    int rc, n, i, success;
    int32_t int_param;
    float float_param;
    bool bool_param;
    bool per_session = false;
    
    if (3 != opal_argv_count(argv)) {
        fprintf(stderr, "incorrect arguments to \"power get\"\n");
        return ORCM_ERR_BAD_PARAM;
    }
  
    if (ORCM_GET_POWER_MODES_COMMAND == cmd) {
        printf("Possible Modes: {%s", orcm_pwrmgmt_get_mode_string(0));
        for(i = 1; i < ORCM_PWRMGMT_NUM_MODES; i++) {
            printf(", %s",orcm_pwrmgmt_get_mode_string(i));
        }
        printf("}\n");
        return ORCM_SUCCESS;
    }
          

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_SCD,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);
    
    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    
    command = ORCM_GET_POWER_COMMAND;
    /* pack the command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    
    command = cmd;
    /* pack the sub-command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,
                                            1, ORCM_SCD_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* This is a global power setting, not per session */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &per_session,
                                            1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER,
                                                      buf,
                                                      ORCM_RML_TAG_SCD,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    /* get result */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &success,
                                              &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    if(OPAL_SUCCESS != success) {
        ORTE_ERROR_LOG(success);
        OBJ_DESTRUCT(&xfer);
        return success;
    }
 
    switch(cmd) {
    case ORCM_GET_POWER_BUDGET_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current cluster power budget: %d watts\n", int_param);
    break;
    case ORCM_GET_POWER_MODE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power mode: %s\n", orcm_pwrmgmt_get_mode_string(int_param));
    break;
    case ORCM_GET_POWER_WINDOW_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power window: %d ms\n", int_param);
    break;
    case ORCM_GET_POWER_OVERAGE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power budget overage limit: %d watts\n", int_param);
    break;
    case ORCM_GET_POWER_UNDERAGE_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power budget underage limit: %d watts\n", int_param);
    break;
    case ORCM_GET_POWER_OVERAGE_TIME_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power overage time limit: %d ms\n", int_param);
    break;
    case ORCM_GET_POWER_UNDERAGE_TIME_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &int_param,
                                                  &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default power underage time limit: %d ms\n", int_param);
    break;
    case ORCM_GET_POWER_FREQUENCY_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &float_param,
                                                  &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        if(fabsf((float)(float_param - (float)ORCM_PWRMGMT_MIN_FREQ)) < 0.0001) {
            printf("Current default power frequency: MAX_FREQ");
        }
        else if(fabsf((float)(float_param - (float)ORCM_PWRMGMT_MIN_FREQ)) < 0.0001) {
            printf("Current default power frequency: MIN_FREQ");
        }
        else {
            printf("Current default power frequency: %f GHz\n", float_param);
        }
    break;
    case ORCM_GET_POWER_STRICT_COMMAND:
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &bool_param,
                                                  &n, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&xfer);
            return rc;
        }
        printf("Current default frequency strict setting: %s\n", bool_param ? "true" : "false");
    break;
    default:
        printf("Illegal power command: %d\n", cmd);
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return ORTE_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}


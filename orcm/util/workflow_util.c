/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"
#include "orcm/mca/parser/parser.h"
#include "orcm/mca/parser/base/base.h"

#define WORKFLOW_UTIL_DEBUG_OUTPUT 10

opal_list_t* orcm_util_workflow_add_retrieve_workflows_section(const char *file)
{
    opal_list_t *result_list = NULL;
    int file_id;

    if (0 > (file_id = orcm_parser.open(file))) {
        return result_list;
    }

    result_list = orcm_parser.retrieve_section(file_id, "workflows", "");
    if (NULL == result_list) {
        orcm_parser.close(file_id);
        return result_list;
    }

    orcm_parser.close(file_id);

    return result_list;
}

static int orcm_util_workflow_add_check_filter_step(char *key, char *value,
                                                    bool *is_filter_first_step)
{
    if ( (false == *is_filter_first_step) && (0 == strcmp("step", key)) ) {
        if (0 == strcmp("filter", value)) {
            *is_filter_first_step = true;
        }
        else {
            opal_output(0, "%s workflow:util:Filter plugin not present",
                       ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORCM_ERR_BAD_PARAM;
        }
    }
    return ORCM_SUCCESS;
}

static int orcm_util_workflow_add_check_name_key(opal_buffer_t *buf, char *list_head_key,
                                                 char *key, bool *is_name_first_key)
{
    int rc;

    if (false == *is_name_first_key) {
        if (0 == strcmp("name", key)) {
            *is_name_first_key = true;
        }
        else {
            opal_output(0, "%s workflow:util:'name' key is missing in 'workflow or 'step' groups",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
             return ORCM_ERR_BAD_PARAM;
        }
    }

    if (0 != strcmp("name", key)) {
        if (0 == strcmp("workflow", list_head_key)) {
            opal_output(0, "%s workflow:util:Wrong tag in workflow block",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORCM_ERR_BAD_PARAM;
        }
        if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &key, 1, OPAL_STRING))) {
            return rc;
        }
    }
    return ORCM_SUCCESS;
}

static int orcm_util_workflow_add_extract_string_type (opal_buffer_t *buf, char *key,
                                                       char *value, char *list_head_key,
                                                       bool *is_name_first_key,
                                                       bool *is_filter_first_step)
{
    int rc;

    rc = orcm_util_workflow_add_check_name_key(buf, list_head_key, key, is_name_first_key);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    rc = orcm_util_workflow_add_check_filter_step(list_head_key, value, is_filter_first_step);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }

    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &value, 1, OPAL_STRING))) {
        return rc;
    }
    return ORCM_SUCCESS;
}

int orcm_util_workflow_add_extract_workflow_info(opal_list_t *result_list, opal_buffer_t *buf,
                                                 char *list_head_key, bool *is_filter_first_step)
{
    orcm_value_t *list_item = NULL;
    int rc;
    int list_length = 0;
    bool is_name_first_key = false;

    if (NULL == result_list){
        return ORCM_ERR_BAD_PARAM;
    }

    list_length = (int)result_list->opal_list_length - 1;

    if (ORCM_SUCCESS != (rc = opal_dss.pack(buf, &list_length, 1, OPAL_INT))) {
        return rc;
    }

    rc = ORCM_ERR_BAD_PARAM;
    OPAL_LIST_FOREACH(list_item, result_list, orcm_value_t) {

        if (list_item->value.type == OPAL_STRING) {
            rc = orcm_util_workflow_add_extract_string_type(buf, list_item->value.key,
                                                            list_item->value.data.string,
                                                            list_head_key, &is_name_first_key,
                                                            is_filter_first_step);
            if (ORCM_SUCCESS != rc) {
                return rc;
            }

        } else if (list_item->value.type == OPAL_PTR) {
            if (false == is_name_first_key) {
                opal_output(0, "%s workflow:util:'name' key is missing in 'workflow or 'step' groups",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
                return ORCM_ERR_BAD_PARAM;
            }
            if (0 == strcmp(list_item->value.key, "step")) {
                rc = orcm_util_workflow_add_extract_workflow_info((opal_list_t*)list_item->value.data.ptr,
                                                                  buf, list_item->value.key,
                                                                  is_filter_first_step);
                if (ORCM_SUCCESS != rc) {
                    return rc;
                }
            }
            else {
                opal_output(0, "%s workflow:util: Unexpected Key in workflow file",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
                return ORCM_ERR_BAD_PARAM;
            }
        } else {
            opal_output(0, "%s workflow:util: Unexpected data type from parser framework",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORCM_ERR_BAD_PARAM;
        }
    }
    return rc;
}

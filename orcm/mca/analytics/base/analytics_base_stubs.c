/*
 * Copyright (c) 2014-2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#define _GNU_SOURCE
#include <ctype.h>

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/dss/dss.h"
#include "opal/threads/threads.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/name_fns.h"

#include "opal/runtime/opal_progress_threads.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/util/utils.h"

#define WORKFLOW_WILD_CHARACTER "*"

static orcm_workflow_t* orcm_analytics_base_workflow_object_init(int *wfid, char *wf_name);
static int orcm_analytics_base_workflow_step_add(orcm_workflow_t *wf, opal_buffer_t* buffer, int step);
static int orcm_analytics_base_add_new_workflow_event_base(orcm_workflow_t *wf);
static int orcm_analytics_base_add_event_base(orcm_workflow_t *wf, char *threadname);
static int orcm_analytics_base_parse_attributes(opal_list_t *attr_list, opal_buffer_t* buffer, int num_attributes);
static int orcm_analytics_base_append_attributes(opal_list_t *attr_list, char *attribute_key, char *attribute_value);
static void orcm_analytics_base_set_event_workflow_step(orcm_workflow_t *wf,
                                                        orcm_workflow_step_t *wf_step,
                                                        orcm_workflow_caddy_t *caddy);
static int orcm_analytics_base_workflow_list_append_info(opal_buffer_t *buffer);
static void orcm_analytics_base_workflow_step_error(int rc, orcm_workflow_step_t *wf_step);


static int workflow_id = 0;

#ifdef ANALYTICS_TAP_INFO
static void orcm_analytics_base_tapinfo(orcm_workflow_step_t *wf_step,
                                        opal_list_t *data)
{
    int rc;
    char *taphost = NULL;
    orte_rml_tag_t taptag = 0;
    orte_process_name_t tapproc;
    opal_buffer_t *buf = NULL;
    opal_value_t *attr = NULL;


    OPAL_LIST_FOREACH(attr, &wf_step->attributes, opal_value_t) {
        if ((0 == strncmp("taphost", attr->key, 10)) && (!taphost)) {
            taphost = strdup(attr->data.string);
        }
        if (0 == strncmp("taptag", attr->key, 10)) {
            taptag = (orte_rml_tag_t)strtol(attr->data.string, NULL, 10);
        }
    }
    if (taptag && taphost) {
        /* pack data */
        buf = OBJ_NEW(opal_buffer_t);
        /* size */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &data->array_size,1, OPAL_SIZE))) {
            ORTE_ERROR_LOG(rc);
        }

        /* data */
        if ((OPAL_SUCCESS == rc) &&
            (OPAL_SUCCESS !=
             (rc = opal_dss.pack(buf, &data->array_items,
                                 data->array_size, OPAL_VALUE)))) {
            ORTE_ERROR_LOG(rc);
        }

        if (OPAL_SUCCESS == rc) {
            if (OPAL_SUCCESS == (rc = orte_util_convert_string_to_process_name(&tapproc, taphost))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                     "%s analytics:base:stubs sending tap to %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&tapproc)));
                if (ORTE_SUCCESS !=
                    (rc = orte_rml.send_buffer_nb(&tapproc, buf, taptag,
                                                  orte_rml_send_callback, NULL))) {
                    ORTE_ERROR_LOG(rc);
                }
            } else {
                ORTE_ERROR_LOG(rc);
            }
        }
        SAFE_RELEASE(buf);
        free(taphost);
    } else {
        if (taphost) {
            free(taphost);
        }
    }
}
#endif

orcm_workflow_caddy_t* orcm_analytics_base_create_caddy(orcm_workflow_t *wf,
                                                        orcm_workflow_step_t *wf_step,
                                                        uint64_t hash_key,
                                                        orcm_analytics_value_t *data)
{
    orcm_workflow_caddy_t *caddy = NULL;

    caddy = OBJ_NEW(orcm_workflow_caddy_t);
    if (NULL == caddy) {
        return NULL;
    }

    OBJ_RETAIN(wf);
    caddy->wf = wf;

    OBJ_RETAIN(wf_step);
    caddy->wf_step = wf_step;

    caddy->hash_key = hash_key;

    /* data was retain'd before it got here */
    caddy->analytics_value = data;
    caddy->imod = wf_step->mod;

    return caddy;
}

static void orcm_analytics_base_set_event_workflow_step(orcm_workflow_t *wf,
                                                        orcm_workflow_step_t *wf_step,
                                                        orcm_workflow_caddy_t *caddy)
{
    orcm_analytics_base_module_t *module = (orcm_analytics_base_module_t *)wf_step->mod;
    if(NULL != module) {
        opal_event_set(wf->ev_base, &caddy->ev, -1,
                       OPAL_EV_WRITE, module->analyze, caddy);
        opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
    }
}

void orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t *wf,
                                                          orcm_workflow_step_t *wf_step,
                                                          uint64_t hash_key,
                                                          orcm_analytics_value_t *data)
{
    orcm_workflow_caddy_t *caddy = NULL;

    caddy = orcm_analytics_base_create_caddy(wf, wf_step, hash_key, data);

    if (NULL == caddy) {
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:caddy can't be created, do nothing!",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return;
    }

#ifdef ANALYTICS_TAP_INFO
    orcm_analytics_base_tapinfo(wf_step, data);
#endif

    orcm_analytics_base_set_event_workflow_step(wf, wf_step, caddy);
}


static int orcm_analytics_base_append_attributes(opal_list_t *attr_list, char *attribute_key, char *attribute_value)
{
    opal_value_t *wf_attribute = NULL;

    wf_attribute = OBJ_NEW(opal_value_t);
    if (NULL == wf_attribute) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    wf_attribute->type = OPAL_STRING;
    wf_attribute->key = attribute_key;
    wf_attribute->data.string = attribute_value;
    opal_list_append(attr_list, &wf_attribute->super);

    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:stubs parse_attributes got attr with key:%s and value:%s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         attribute_key, attribute_value));
    return ORCM_SUCCESS;
}

static int orcm_analytics_base_parse_attributes(opal_list_t *attr_list, opal_buffer_t* buffer, int num_attributes)
{
    int rc;
    int temp = 0;
    char *attribute_key = NULL;
    char *attribute_value = NULL;
    int cnt = ANALYTICS_COUNT_DEFAULT;

    for (temp = 0; temp < num_attributes; temp++ ) {

        rc = opal_dss.unpack(buffer, &attribute_key, &cnt, OPAL_STRING);
        if (OPAL_SUCCESS != rc) {
            return ORCM_ERROR;
        }

        rc = opal_dss.unpack(buffer, &attribute_value, &cnt, OPAL_STRING);
        if (OPAL_SUCCESS != rc) {
            return ORCM_ERROR;
        }
        rc = orcm_analytics_base_append_attributes(attr_list, attribute_key, attribute_value);
        if (ORCM_SUCCESS != rc) {
            return rc;
        }
    }
    return ORCM_SUCCESS;
}

static int orcm_analytics_base_add_event_base(orcm_workflow_t *wf, char *threadname)
{

    wf->ev_active = true;
    if (NULL == (wf->ev_base = opal_progress_thread_init(threadname))) {
        wf->ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    return ORCM_SUCCESS;
}

static int orcm_analytics_base_add_new_workflow_event_base(orcm_workflow_t *wf)
{
    int rc;
    char *threadname = NULL;

    asprintf(&threadname, "wfid%i", wf->workflow_id);
    rc = orcm_analytics_base_add_event_base(wf, threadname);
    SAFEFREE(threadname);

    return rc;
}

static void orcm_analytics_base_workflow_step_error(int rc, orcm_workflow_step_t *wf_step)
{
    ORTE_ERROR_LOG(rc);
    SAFE_RELEASE(wf_step);
}

static int orcm_analytics_base_workflow_step_add(orcm_workflow_t *wf, opal_buffer_t* buffer, int step)
{
    orcm_workflow_step_t *wf_step = NULL;
    int rc;
    char *step_name = NULL;
    int cnt = ANALYTICS_COUNT_DEFAULT;
    int num_attributes;

    wf_step = OBJ_NEW(orcm_workflow_step_t);
    if (NULL == wf_step) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    wf_step->step_id = step;

    rc = opal_dss.unpack(buffer, &num_attributes, &cnt, OPAL_INT);
    if (OPAL_SUCCESS != rc) {
        orcm_analytics_base_workflow_step_error(rc, wf_step);
        return rc;
    }

    rc = opal_dss.unpack(buffer, &step_name, &cnt, OPAL_STRING);
    if (OPAL_SUCCESS != rc) {
        orcm_analytics_base_workflow_step_error(rc, wf_step);
        return rc;
    }
    wf_step->analytic = step_name;

    rc = orcm_analytics_base_parse_attributes(&wf_step->attributes, buffer, num_attributes);
    if (ORCM_SUCCESS != rc ) {
        orcm_analytics_base_workflow_step_error(rc, wf_step);
        return rc;
    }

    rc = orcm_analytics_base_select_workflow_step(wf_step);
    if (ORCM_SUCCESS != rc) {
        orcm_analytics_base_workflow_step_error(rc, wf_step);
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:workflow step can't be created",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_ERROR;
    }
    opal_list_append(&wf->steps, &wf_step->super);
    return ORCM_SUCCESS;
}


static orcm_workflow_t* orcm_analytics_base_workflow_object_init(int *wfid, char *wf_name)
{
    orcm_workflow_t *wf = NULL;

    wf = OBJ_NEW(orcm_workflow_t);
    if (NULL == wf) {
        return NULL;
    }

    wf->workflow_id = workflow_id;
    wf->name = wf_name;
    workflow_id++;
    *wfid = wf->workflow_id;

    return wf;
}


int orcm_analytics_base_workflow_add(opal_buffer_t* buffer, int *wfid)
{
    int num_steps;
    int step;
    int cnt;
    int rc = ORCM_SUCCESS;
    orcm_workflow_t *wf = NULL;
    char *wf_name = NULL;

    /* unpack the number of workflow steps */
    cnt = ANALYTICS_COUNT_DEFAULT;
    rc = opal_dss.unpack(buffer, &num_steps, &cnt, OPAL_INT);
    if (OPAL_SUCCESS != rc) {
        goto error;
    }

    rc = opal_dss.unpack(buffer, &wf_name, &cnt, OPAL_STRING);
    if (OPAL_SUCCESS != rc) {
        goto error;
    }

    if (NULL == (wf = orcm_analytics_base_workflow_object_init(wfid, wf_name))) {
        rc = ORCM_ERROR;
        SAFEFREE(wf_name);
        goto error;
    }

    rc = orcm_analytics_base_add_new_workflow_event_base(wf);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        goto error;
    }

    /* create each step (module) of the workflow */
    for (step = 0; step < num_steps; step = step + 1) {
        rc = orcm_analytics_base_workflow_step_add(wf, buffer, step);
        if (OPAL_SUCCESS != rc) {
            goto error;
        }
    }

    /* add workflow to the master list of workflows */
    opal_list_append(&orcm_analytics_base.workflows, &wf->super);
    return ORCM_SUCCESS;

error:
    SAFE_RELEASE(wf);
    return rc;
}

int orcm_analytics_base_workflow_remove(opal_buffer_t *buffer)
{
    orcm_workflow_t *wf = NULL;
    orcm_workflow_t *next = NULL;
    char *workflow_id = NULL;
    char *workflow_name = NULL;
    int cnt = ANALYTICS_COUNT_DEFAULT;
    int workflow_num_id = -1;
    int rc;
    bool is_workflow_deleted = false;

    rc = opal_dss.unpack(buffer, &workflow_name, &cnt, OPAL_STRING);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }

    rc = opal_dss.unpack(buffer, &workflow_id, &cnt, OPAL_STRING);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }

    if (0 != strcmp(workflow_id, WORKFLOW_WILD_CHARACTER)) {
        if (0 != isdigit(workflow_id[strlen(workflow_id)-1])) {
            workflow_num_id = (int)strtol(workflow_id, NULL, 10);
        }
    }

    OPAL_LIST_FOREACH_SAFE(wf, next, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if ( (0 == strcmp(workflow_name, WORKFLOW_WILD_CHARACTER)) ||
                ( (0 == strcmp(workflow_name, wf->name)) &&
                ((workflow_num_id == wf->workflow_id) ||
                        (0 == strcmp(workflow_id, WORKFLOW_WILD_CHARACTER))) ) ) {
            /* stop the event thread */
            orcm_analytics_stop_wokflow(wf);

            /* remove workflow from the master list */
            opal_list_remove_item(&orcm_analytics_base.workflows, &wf->super);
            SAFE_RELEASE(wf);
            is_workflow_deleted = true;
        }
    }
    if (true == is_workflow_deleted) {
        return ORCM_SUCCESS;
    }
    else {
        return ORCM_ERROR;
    }
}

static int orcm_analytics_base_workflow_list_append_info(opal_buffer_t *buffer)
{
    orcm_workflow_t *wf = NULL;
    int rc;
    char *wf_name = NULL;

    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        rc = orcm_analytics_base_recv_pack_int(buffer, (int *)&wf->workflow_id, ANALYTICS_COUNT_DEFAULT);
        if (ORCM_SUCCESS != rc) {
            return rc;
        }

        wf_name = strdup(wf->name);
        if (NULL == wf_name) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        rc = opal_dss.pack(buffer, &wf_name, 1, OPAL_STRING);
        if (ORCM_SUCCESS != rc) {
            SAFEFREE(wf_name);
            return rc;
        }
        SAFEFREE(wf_name);
    }
    return ORCM_SUCCESS;
}

int orcm_analytics_base_workflow_list(opal_buffer_t *buffer)
{
    size_t workflow_list_size = 0;
    int rc;

    workflow_list_size = opal_list_get_size(&orcm_analytics_base.workflows);
    rc = orcm_analytics_base_recv_pack_int(buffer, (int *)&workflow_list_size, ANALYTICS_COUNT_DEFAULT);
    if (ORCM_SUCCESS != rc) {
        return rc;
    }
    if (0 < workflow_list_size) {
        rc = orcm_analytics_base_workflow_list_append_info(buffer);
    }

    return rc;
}


void orcm_analytics_base_send_data(orcm_analytics_value_t *data)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_t *wf = NULL;
    orcm_ras_event_t *analytics_event_data = NULL;

    if (true == orcm_analytics_base.store_raw_data) {
        analytics_event_data = orcm_analytics_base_event_create(data, ORCM_RAS_EVENT_SENSOR, ORCM_RAS_SEVERITY_INFO);
        if (NULL != analytics_event_data) {
            rc = orcm_analytics_base_event_set_storage(analytics_event_data, ORCM_STORAGE_TYPE_DATABASE);
            if(ORCM_SUCCESS != rc){
                OBJ_RELEASE(analytics_event_data);
                return;
            }
            ORCM_RAS_EVENT(analytics_event_data);
        }
    }

    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if (NULL == data) {
            break;
        }
        OBJ_RETAIN(data);
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(wf,(void*)(&(wf->steps.opal_list_sentinel)), 0, data, NULL);
    }

}

int orcm_analytics_base_log_to_database_event(orcm_analytics_value_t* value)
{
    int rc = ORCM_SUCCESS;
    orcm_ras_event_t *event_data = NULL;
    event_data = orcm_analytics_base_event_create(value,ORCM_RAS_EVENT_SENSOR, ORCM_RAS_SEVERITY_INFO);
    if(NULL == event_data){
        rc = ORCM_ERROR;
        return rc;
    }
    rc = orcm_analytics_base_event_set_storage(event_data, ORCM_STORAGE_TYPE_DATABASE);
    if(ORCM_SUCCESS != rc){
        if (NULL != event_data) {
            SAFE_RELEASE(event_data);
        }
        return rc;
    }
    ORCM_RAS_EVENT(event_data);
    return rc;
}

int orcm_analytics_base_assert_caddy_data(void *cbdata)
{
    orcm_workflow_caddy_t *current_caddy = NULL;

    if (NULL == cbdata) {
        return ORCM_ERR_BAD_PARAM;
    }

    current_caddy = (orcm_workflow_caddy_t *)cbdata;
    if (NULL == current_caddy->imod || NULL == current_caddy->wf ||
        NULL == current_caddy->wf_step || NULL == current_caddy->analytics_value) {
        return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;
}

uint64_t orcm_analytics_base_timeval_to_uint64(struct timeval time)
{
    uint64_t time_sec = (uint64_t)time.tv_sec;
    double time_usec = (double)time.tv_usec / 10E6;
    if (0.5 <= time_usec) {
        time_sec++;
    }
    return time_sec;
}

int orcm_analytics_base_get_sample_time(opal_list_t *list, uint64_t *sample_time)
{
    orcm_value_t *value_item = NULL;
    bool found_time = false;

    OPAL_LIST_FOREACH(value_item, list, orcm_value_t) {
        if (NULL == value_item || NULL == value_item->value.key) {
            return ORCM_ERR_BAD_PARAM;
        }
        if (NULL != strstr(value_item->value.key, "time")) {
            found_time = true;
            break;
        }
    }
    if (found_time) {
        *sample_time = orcm_analytics_base_timeval_to_uint64(value_item->value.data.tv);
        return ORCM_SUCCESS;
    }

    return ORCM_ERR_BAD_PARAM;
}

int orcm_analytics_base_get_compute_type(char* compute_type)
{
    int compute_type_id = ORCM_ANALYTICS_COMPUTE_UNKNOWN;
    if (NULL != compute_type) {
        if (0 == strncmp(compute_type, ORCM_ANALYTICS_COMPUTE_AVE_STR,
                 strlen(ORCM_ANALYTICS_COMPUTE_AVE_STR) + 1)) {
            compute_type_id = ORCM_ANALYTICS_COMPUTE_AVE;
        } else if (0 == strncmp(compute_type, ORCM_ANALYTICS_COMPUTE_MIN_STR,
                   strlen(ORCM_ANALYTICS_COMPUTE_MIN_STR) + 1)) {
            compute_type_id = ORCM_ANALYTICS_COMPUTE_MIN;
        } else if (0 == strncmp(compute_type, ORCM_ANALYTICS_COMPUTE_MAX_STR,
                   strlen(ORCM_ANALYTICS_COMPUTE_MAX_STR) + 1)) {
            compute_type_id = ORCM_ANALYTICS_COMPUTE_MAX;
        } else if (0 == strncmp(compute_type, ORCM_ANALYTICS_COMPUTE_SD_STR,
                   strlen(ORCM_ANALYTICS_COMPUTE_SD_STR) + 1)) {
            compute_type_id = ORCM_ANALYTICS_COMPUTE_SD;
        }
    }
    return compute_type_id;
}

char* orcm_analytics_base_set_compute_type(int compute_type_id)
{
    char* compute_type = NULL;

    switch (compute_type_id) {
        case ORCM_ANALYTICS_COMPUTE_AVE:
            compute_type = ORCM_ANALYTICS_COMPUTE_AVE_STR;
            break;
        case ORCM_ANALYTICS_COMPUTE_MIN:
            compute_type = ORCM_ANALYTICS_COMPUTE_MIN_STR;
            break;
        case ORCM_ANALYTICS_COMPUTE_MAX:
            compute_type = ORCM_ANALYTICS_COMPUTE_MAX_STR;
            break;
        case ORCM_ANALYTICS_COMPUTE_SD:
            compute_type = ORCM_ANALYTICS_COMPUTE_SD_STR;
            break;
        default:
            compute_type = ORCM_ANALYTICS_COMPUTE_UNKNOW_STR;
            break;
    }
    return compute_type;
}

char* orcm_analytics_get_hostname_from_attributes(opal_list_t* attributes)
{
    char* hostname = NULL;
    orcm_value_t* attribute = NULL;
    OPAL_LIST_FOREACH(attribute, attributes, orcm_value_t) {
        if(NULL != attribute->value.key){
            if(0 == strcmp(attribute->value.key,"hostname")){
                hostname = strdup(attribute->value.data.string);
                break;
            }
        }
    }
    if(NULL == hostname){
        hostname = strdup("All");
    }
    return hostname;
}

/**
 * Generate a data key identified by a given workflow and workflow step.
 */
int orcm_analytics_base_data_key(char** data_key, char* key, orcm_workflow_t* wf,
                                 orcm_workflow_step_t* wf_step)
{
        return asprintf(data_key, "%s_workflow%d-step%d_%s:%s_%s", wf->name, wf->workflow_id,
                        wf_step->step_id, wf_step->analytic, key, wf->hostname_regex);
}

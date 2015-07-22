/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/dss/dss.h"
#include "opal/threads/threads.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/name_fns.h"

#include "orcm/runtime/orcm_progress.h"

#include "orcm/mca/analytics/base/base.h"
#include "orcm/mca/analytics/base/analytics_private.h"


static orcm_workflow_t* orcm_analytics_base_workflow_object_init(int *wfid);
static int orcm_analytics_base_workflow_step_create(orcm_workflow_t *wf,
                                                    opal_value_t **values, int i);
static int orcm_analytics_base_create_new_workflow_event_base(orcm_workflow_t *wf);
static int orcm_analytics_base_create_event_base(orcm_workflow_t *wf, char *threadname);
static void* orcm_analytics_base_progress_thread_engine(opal_object_t *obj);
static int orcm_analytics_base_parse_attributes(opal_list_t *attr_list, char *attr_string);
static int orcm_analytics_base_subtokenize_attributes(char **tokens, opal_list_t *attr_list);
static void orcm_analytics_base_append_attributes(char **subtokens, opal_list_t *attr_list);
static void orcm_analytics_base_set_event_workflow_step(orcm_workflow_t *wf,
                                                        orcm_workflow_step_t *wf_step,
                                                        orcm_workflow_caddy_t *caddy);
static orcm_workflow_caddy_t* orcm_analytics_base_create_caddy(orcm_workflow_t *wf,
                                                               orcm_workflow_step_t *wf_step,
                                                               opal_value_array_t *data);

static int workflow_id = 0;


#ifdef ANALYTICS_TAP_INFO
static void orcm_analytics_base_tapinfo(orcm_workflow_step_t *wf_step,
                                        opal_value_array_t *data)
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
        OBJ_RELEASE(buf);
        free(taphost);
    } else {
        if (taphost) {
            free(taphost);
        }
    }
}
#endif

static orcm_workflow_caddy_t* orcm_analytics_base_create_caddy(orcm_workflow_t *wf,
                                                               orcm_workflow_step_t *wf_step,
                                                               opal_value_array_t *data)
{
    orcm_workflow_caddy_t *caddy = NULL;

    caddy = OBJ_NEW(orcm_workflow_caddy_t);

    OBJ_RETAIN(wf);
    caddy->wf = wf;

    OBJ_RETAIN(wf_step);
    caddy->wf_step = wf_step;

    /* data was retain'd before it got here */
    caddy->data = data;
    caddy->imod = wf_step->mod;

    return caddy;
}

static void orcm_analytics_base_set_event_workflow_step(orcm_workflow_t *wf,
                                                        orcm_workflow_step_t *wf_step,
                                                        orcm_workflow_caddy_t *caddy)
{
    orcm_analytics_base_module_t *module = (orcm_analytics_base_module_t *)wf_step->mod;

    opal_event_set(wf->ev_base, &caddy->ev, -1,
                   OPAL_EV_WRITE, module->analyze, caddy);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}

void orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t *wf,
                                                          orcm_workflow_step_t *wf_step,
                                                          opal_value_array_t *data)
{
    orcm_workflow_caddy_t *caddy = NULL;

    caddy = orcm_analytics_base_create_caddy(wf, wf_step, data);

#ifdef ANALYTICS_TAP_INFO
    orcm_analytics_base_tapinfo(wf_step, data);
#endif

    orcm_analytics_base_set_event_workflow_step(wf, wf_step, caddy);
}


static void orcm_analytics_base_append_attributes(char **subtokens, opal_list_t *attr_list)
{
    opal_value_t *wf_attribute = NULL;
    char *output = NULL;

    wf_attribute = OBJ_NEW(opal_value_t);
    wf_attribute->type = OPAL_STRING;
    wf_attribute->key = subtokens[0];
    wf_attribute->data.string = subtokens[1];
    opal_list_append(attr_list, &wf_attribute->super);
    opal_dss.print(&output, "", wf_attribute, OPAL_VALUE);
    OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                         "%s analytics:base:stubs parse_attributes got attr %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         output));
    free(output);
}


static int orcm_analytics_base_subtokenize_attributes(char **tokens, opal_list_t *attr_list)
{
    char **subtokens = NULL;
    int i;
    int array_length;
    int subarray_length = 0;


    array_length = opal_argv_count(tokens);

    for (i = 0; i < array_length; i++) {
        subtokens = opal_argv_split(tokens[i], '=');
        subarray_length = opal_argv_count(subtokens);

        if (subarray_length == MAX_ALLOWED_ATTRIBUTES_PER_WORKFLOW_STEP) {
            orcm_analytics_base_append_attributes(subtokens, attr_list );

        } else {
            opal_argv_free(subtokens);
            return ORCM_ERR_BAD_PARAM;
        }
    }
    return ORCM_SUCCESS;
}

static int orcm_analytics_base_parse_attributes(opal_list_t *attr_list, char *attr_string)
{
    char **tokens = NULL;
    int ret = ORCM_SUCCESS;

    if (NULL == attr_string){
        return ORCM_SUCCESS;
    }
    tokens = opal_argv_split(attr_string, ',');

    ret = orcm_analytics_base_subtokenize_attributes(tokens, attr_list);

    opal_argv_free(tokens);
    return ret;
}

static void* orcm_analytics_base_progress_thread_engine(opal_object_t *obj)
{
    opal_thread_t *thread = (opal_thread_t *)obj;
    orcm_workflow_t *wf = (orcm_workflow_t *)thread->t_arg;

    while (true == wf->ev_active) {
        opal_event_loop(wf->ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}


static int orcm_analytics_base_create_event_base(orcm_workflow_t *wf, char *threadname)
{

    wf->ev_active = true;
    if (NULL ==
        (wf->ev_base = orcm_start_progress_thread(threadname,
                                                  orcm_analytics_base_progress_thread_engine,
                                                  (void *)wf))) {
        wf->ev_active = false;
        free(threadname);
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    return ORCM_SUCCESS;
}

static int orcm_analytics_base_create_new_workflow_event_base(orcm_workflow_t *wf)
{
    int rc;
    char *threadname = NULL;

    asprintf(&threadname, "wfid%i", wf->workflow_id);
    rc = orcm_analytics_base_create_event_base(wf, threadname);
    free(threadname);

    return rc;
}


static int orcm_analytics_base_workflow_step_create(orcm_workflow_t *wf,
                                                    opal_value_t **values, int i)
{
    orcm_workflow_step_t *wf_step = NULL;
    int rc;

    wf_step = OBJ_NEW(orcm_workflow_step_t);

    wf_step->analytic = strdup(values[i]->data.string);

    if (NULL != values[i+1]) {
        rc = orcm_analytics_base_parse_attributes(&wf_step->attributes, values[i+1]->data.string);
        if (ORCM_SUCCESS != rc ) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(wf_step);
            return rc;
        }
    }

    rc = orcm_analytics_base_select_workflow_step(wf_step);
    if (ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(wf_step);
        OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                             "%s analytics:base:workflow step can't be created",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ORCM_SUCCESS;
    }
    opal_list_append(&wf->steps, &wf_step->super);
    return ORCM_SUCCESS;
}


static orcm_workflow_t* orcm_analytics_base_workflow_object_init(int *wfid)
{
    orcm_workflow_t *wf = NULL;

    wf = OBJ_NEW(orcm_workflow_t);
    wf->workflow_id = workflow_id;
    workflow_id++;
    *wfid = wf->workflow_id;

    return wf;
}


int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid)
{
    int num_steps;
    int step;
    int cnt;
    int rc = ORCM_SUCCESS;
    orcm_workflow_t *wf = NULL;
    opal_value_t **values;

    /* unpack the number of workflow steps */
    cnt = ANALYTICS_COUNT_DEFAULT;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &num_steps,
                                              &cnt, OPAL_INT))) {
        goto error;
    }

    if (NULL == (wf = orcm_analytics_base_workflow_object_init(wfid))) {
        goto error;
    }


    if (ORCM_SUCCESS != (rc = orcm_analytics_base_create_new_workflow_event_base(wf))) {
        ORTE_ERROR_LOG(rc);
        goto error;
    }


    cnt = MAX_ALLOWED_ATTRIBUTES_PER_WORKFLOW_STEP * num_steps;
    values = (opal_value_t**)malloc(cnt * sizeof(opal_value_t *));

    /* unpack the requested module name and attributes stored in opal_values */
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, values, &cnt, OPAL_VALUE))) {
        ORTE_ERROR_LOG(rc);
        goto error;
    }


    /* create each step (module) of the workflow */
    for (step = 0; step < cnt; step = step + MAX_ALLOWED_ATTRIBUTES_PER_WORKFLOW_STEP) {
        if (OPAL_SUCCESS != (rc = orcm_analytics_base_workflow_step_create(wf, values, step))) {
            goto error;
        }

    }

    free(values);

    /* add workflow to the master list of workflows */
    opal_list_append(&orcm_analytics_base_wf.workflows, &wf->super);
    return ORCM_SUCCESS;

error:
    if (NULL != buffer) {
        OBJ_RELEASE(buffer);
    }
    if (NULL != wf) {
        OBJ_RELEASE(wf);
    }
    return rc;
}

int orcm_analytics_base_workflow_delete(int workflow_id)
{
    orcm_workflow_t *wf = NULL;
    orcm_workflow_t *next = NULL;

    OPAL_LIST_FOREACH_SAFE(wf, next, &orcm_analytics_base_wf.workflows, orcm_workflow_t) {
        if (workflow_id == wf->workflow_id) {
            /* stop the event thread */
            orcm_analytics_stop_wokflow(wf);

            /* remove workflow from the master list */
            opal_list_remove_item(&orcm_analytics_base_wf.workflows, &wf->super);
            OBJ_RELEASE(wf);
        }
    }
    return ORCM_SUCCESS;
}

void orcm_analytics_base_send_data(opal_value_array_t *data)
{
    orcm_workflow_t *wf = NULL;

    OPAL_LIST_FOREACH(wf, &orcm_analytics_base_wf.workflows, orcm_workflow_t) {
        OBJ_RETAIN(data);
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(wf,(&(wf->steps.opal_list_sentinel)), data);
    }
}

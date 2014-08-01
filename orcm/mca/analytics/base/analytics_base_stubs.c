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

static int parse_attributes(opal_list_t *attr_list, char *attr_string);
static void* progress_thread_engine(opal_object_t *obj);

static int workflow_id = 0;

void orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t *wf,
                                                          orcm_workflow_step_t *wf_step,
                                                          opal_value_array_t *data) {
    orcm_workflow_caddy_t *caddy;
    orcm_analytics_base_module_t *module = (orcm_analytics_base_module_t *)wf_step->mod;
    opal_value_t *attr;
    char *taphost = NULL;
    orte_rml_tag_t taptag = 0;
    struct orte_process_name_t tapproc;
    int rc;
    opal_buffer_t *buf;
    
    caddy = OBJ_NEW(orcm_workflow_caddy_t);
    
    OBJ_RETAIN(wf_step);
    caddy->wf_step = wf_step;
    /* data was retain'd before it got here */
    caddy->data = data;
    caddy->imod = wf_step->mod;
    
    OPAL_LIST_FOREACH(attr, &wf_step->attributes, opal_value_t) {
        if (0 == strncmp("taphost", attr->key, 10)) {
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
    }
    
    opal_event_set(wf->ev_base, &caddy->ev, -1,
                   OPAL_EV_WRITE, module->analyze, caddy);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}

static int parse_attributes(opal_list_t *attr_list, char *attr_string) {
    char **tokens = NULL;
    char **subtokens = NULL;
    int i, array_length, subarray_length;
    opal_value_t *tokenized;
    char *output;
    
    tokens = opal_argv_split(attr_string, ',');
    array_length = opal_argv_count(tokens);
    tokenized = (opal_value_t *)malloc(sizeof(opal_value_t));

    for (i = 0; i < array_length; i++) {
        subtokens = opal_argv_split(tokens[i], '=');
        subarray_length = opal_argv_count(subtokens);
        if (2 == subarray_length) {
            tokenized->type = OPAL_STRING;
            tokenized->key = subtokens[0];
            tokenized->data.string = subtokens[1];
            opal_list_append(attr_list, &tokenized->super);
            opal_dss.print(&output, "", tokenized, OPAL_VALUE);
            OPAL_OUTPUT_VERBOSE((5, orcm_analytics_base_framework.framework_output,
                                 "%s analytics:base:stubs parse_attributes got attr %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 output));
            free(output);
        } else {
            return ORCM_ERR_BAD_PARAM;
        }
    }
    free(tokenized);

    return ORCM_SUCCESS;
}

static void* progress_thread_engine(opal_object_t *obj)
{
    opal_thread_t *thread = (opal_thread_t *)obj;
    orcm_workflow_t *wf = (orcm_workflow_t *)thread->t_arg;
    
    while (wf->ev_active) {
        opal_event_loop(wf->ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

int orcm_analytics_base_workflow_create(opal_buffer_t* buffer, int *wfid)
{
    int num_steps, i, cnt, rc;
    char *mod_attrs = NULL;
    orcm_workflow_step_t *wf_step = NULL;
    orcm_workflow_t *wf;
    opal_value_t module_name;
    opal_value_t module_attr;
    opal_value_t **values;
    char *threadname = NULL;
    
    /* unpack the number of steps */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &num_steps,
                                              &cnt, OPAL_INT))) {
        goto error;
    }
    
    wf = OBJ_NEW(orcm_workflow_t);
    wf->workflow_id = workflow_id;
    workflow_id++;
    *wfid = wf->workflow_id;
    
    /* create new event base for workflow */
    asprintf(&threadname, "wfid%i", wf->workflow_id);
    wf->ev_active = true;
    if (NULL ==
        (wf->ev_base = orcm_start_progress_thread(threadname,
                                                  progress_thread_engine,
                                                  (void *)wf))) {
        wf->ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    
    OBJ_CONSTRUCT(&module_name, opal_value_t);
    OBJ_CONSTRUCT(&module_attr, opal_value_t);
    
    values = (opal_value_t**)malloc(2 * sizeof(opal_value_t *));
    values[0] = &module_name;
    values[1] = &module_attr;
    
    /* create each step (module) of the workflow */
    for (i = 0; i < num_steps; i = i+2) {
        wf_step = OBJ_NEW(orcm_workflow_step_t);
        /* unpack the requested module name and attributes stored in opal_values */
        cnt = 2;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, values,
                                                  &cnt, OPAL_VALUE))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(wf_step);
            goto error;
        }
        
        wf_step->analytic = strdup(values[0]->data.string);
        
        if (ORCM_SUCCESS !=
            (rc = parse_attributes(&wf_step->attributes, values[1]->data.string))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(wf_step);
            goto error;
        }
        
        if (ORCM_SUCCESS !=
            (rc = orcm_analytics_base_select_workflow_step(wf_step))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(wf_step);
            free(mod_attrs);
            /* skip this module ? */
            /* FIXME log something */
            continue;
        }
        opal_list_append(&wf->steps, &wf_step->super);
        free(mod_attrs);
    }
    
    OBJ_DESTRUCT(&module_name);
    OBJ_DESTRUCT(&module_attr);
    free(values);
    
    /* add workflow to the master list of workflows */
    opal_list_append(&orcm_analytics_base.workflows, &wf->super);
    
    return ORCM_SUCCESS;

error:
    if (mod_attrs) {
        free(mod_attrs);
    }
    if (buffer) {
        OBJ_RELEASE(buffer);
    }
    if (wf) {
        OBJ_RELEASE(wf);
    }
    return rc;
}

int orcm_analytics_base_workflow_delete(int workflow_id)
{
    orcm_workflow_t *wf;
    orcm_workflow_t *next;
    char *threadname = NULL;
    
    OPAL_LIST_FOREACH_SAFE(wf, next, &orcm_analytics_base.workflows, orcm_workflow_t) {
        if (workflow_id == wf->workflow_id) {
            /* stop the event thread */
            asprintf(&threadname, "wfid%i", wf->workflow_id);
            if (wf->ev_active) {
                wf->ev_active = false;
                orcm_stop_progress_thread(threadname, true);
            }
            /* remove workflow from the master list */
            opal_list_remove_item(&orcm_analytics_base.workflows, &wf->super);
            OBJ_RELEASE(wf);
        }
    }
    return ORCM_SUCCESS;
}

int orcm_analytics_base_send_data(opal_value_array_t *data)
{
    orcm_workflow_t *wf;
    
    OPAL_LIST_FOREACH(wf, &orcm_analytics_base.workflows, orcm_workflow_t) {
        OBJ_RETAIN(data);
        ORCM_ACTIVATE_WORKFLOW_STEP(wf, data);
    }
    return ORCM_SUCCESS;
}

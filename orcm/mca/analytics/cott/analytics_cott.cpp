/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>
#include <ctype.h>

#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/notifier/notifier.h"

#include "orcm/mca/evgen/evgen.h"
#include "orcm/mca/analytics/base/analytics_private.h"
#include "analytics_cott.h"
#include "host_analyze_counters.h"

#define HASH_TABLE_SIZE 10000

// C
#include <time.h>
#include <stdint.h>

// C++
#include <vector>
#include <string>

using namespace std;

int cott_init(orcm_analytics_base_module_t *imod);
void cott_finalize(orcm_analytics_base_module_t *imod);
string get_hostname_from_caddy(orcm_workflow_caddy_t* caddy);
time_t get_timeval_from_caddy(orcm_workflow_caddy_t* caddy);
void add_data_values(const string& hostname, time_t tv, orcm_workflow_caddy_t* caddy,
                     opal_list_t* threshold_list);
int cott_analyze(int sd, short args, void *cbdata);
void cott_event_fired_callback(const string& host, const string& label, uint32_t count, time_t elapsed,
                               const vector<uint32_t>& data, void* cb_data);
void activate_next_wf(orcm_workflow_caddy_t* caddy);
int assert_caddy_data(void *cbdata);
void set_attributes(orcm_workflow_caddy_t* caddy);

#define SAFE_FREE(x) if(NULL != x) { free((void*)x); x=NULL; }
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) OBJ_RELEASE(x)
#define SAFE_DELETE(x) if(NULL != x) { delete x; x=NULL; }

#define ON_NULL_RETURN(x) if(NULL==x) return
#define ON_NULL_BREAK(x) if(NULL==x) break
#define ON_NULL_ALLOC_BREAK(x, e)  if(NULL==x) { e=ORCM_ERR_OUT_OF_RESOURCE; break; }
#define ON_FAILURE_BREAK(x) if(ORCM_SUCCESS!=(x)) break

/* fill the computation data list to be passed to the next plugin */
extern "C" {
    mca_analytics_cott_module_t orcm_analytics_cott_module = {
        {
            cott_init,
            cott_finalize,
            cott_analyze,
            NULL,
        },
    };

    extern orcm_value_t* orcm_util_load_orcm_value(char*, void*, int, char*);

} // extern "C"

host_analyze_counters* counter_analyzer = NULL;

typedef int (*assert_caddy_data_fn_t)(void *cbdata);
assert_caddy_data_fn_t assert_caddy_data_hook = assert_caddy_data;

map<string, unsigned int> lookup_fault;
map<string, bool> lookup_store;
map<string, int> lookup_severity;

struct step_data_t
{
    int fault_type;
    bool store_event;
    int severity;
    bool use_notifier;
    string notifier_action;
};

struct step_data_t global_step_data = {
    ORCM_EVENT_HARD_FAULT,
    true,
    ORCM_RAS_SEVERITY_ERROR,
    false,
    "none"
};

typedef struct {
    orcm_workflow_caddy_t* caddy;
    opal_list_t* threshold_data;
} analyze_data_t;

int cott_init(orcm_analytics_base_module_t *imod)
{
    if (NULL == imod) {
        return ORCM_ERR_BAD_PARAM;
    }
    if(0 == lookup_fault.size()) {
        lookup_fault["soft"] = ORCM_EVENT_SOFT_FAULT;
        lookup_fault["hard"] = ORCM_EVENT_HARD_FAULT;
    }
    if(0 == lookup_store.size()) {
        lookup_store["yes"] = true;
        lookup_store["true"] = true;
        lookup_store["1"] = true;
        lookup_store["no"] = false;
        lookup_store["false"] = false;
        lookup_store["0"] = false;
    }
    if(0 == lookup_severity.size()) {
        lookup_severity["emerg"]  = ORCM_RAS_SEVERITY_EMERG;
        lookup_severity["fatal"]  = ORCM_RAS_SEVERITY_FATAL;
        lookup_severity["alert"]  = ORCM_RAS_SEVERITY_ALERT;
        lookup_severity["crit"]   = ORCM_RAS_SEVERITY_CRIT;
        lookup_severity["error"]  = ORCM_RAS_SEVERITY_ERROR;
        lookup_severity["warn"]   = ORCM_RAS_SEVERITY_WARNING;
        lookup_severity["notice"] = ORCM_RAS_SEVERITY_NOTICE;
        lookup_severity["info"]   = ORCM_RAS_SEVERITY_INFO;
        lookup_severity["trace"]  = ORCM_RAS_SEVERITY_TRACE;
        lookup_severity["debug"]  = ORCM_RAS_SEVERITY_DEBUG;
    }
    int rv;
    opal_hash_table_t* table = NULL;
    mca_analytics_cott_module_t *mod = (mca_analytics_cott_module_t *)imod;
    do {
        mod->api.orcm_mca_analytics_data_store = OBJ_NEW(opal_hash_table_t);
        ON_NULL_ALLOC_BREAK(mod->api.orcm_mca_analytics_data_store, rv);

        table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_data_store;

        if(ORCM_SUCCESS != (rv = opal_hash_table_init(table, HASH_TABLE_SIZE))) {
            break;
        }

        counter_analyzer = new host_analyze_counters((void*)&global_step_data);
        ON_NULL_ALLOC_BREAK(counter_analyzer, rv);
        return ORCM_SUCCESS;
    } while(false);
    SAFE_OBJ_RELEASE(table);
    mod->api.orcm_mca_analytics_data_store = NULL;
    return rv;
}

void cott_finalize(orcm_analytics_base_module_t *imod)
{
    if (NULL != imod) {
        mca_analytics_cott_module_t* mod = (mca_analytics_cott_module_t *)imod;
        opal_hash_table_t* table = (opal_hash_table_t*)mod->api.orcm_mca_analytics_data_store;
        SAFE_OBJ_RELEASE(table);
        SAFE_FREE(mod);
    }
    SAFE_DELETE(counter_analyzer);
}

void cott_event_fired_callback(const string& host, const string& label, uint32_t count, time_t elapsed,
                               const vector<uint32_t>& data, void* cb_data)
{
    analyze_data_t* analyze_data = (analyze_data_t*)cb_data;
    ON_NULL_RETURN(analyze_data);
    orcm_workflow_caddy_t* caddy = analyze_data->caddy;
    ON_NULL_RETURN(caddy);
    opal_list_t* threshold_list = analyze_data->threshold_data;
    ON_NULL_RETURN(threshold_list);
    step_data_t* step_data = (step_data_t*)counter_analyzer->get_user_data();
    ON_NULL_RETURN(step_data);

    char* message = NULL;
    asprintf(&message, "%s analytics:cott:EVENT '%s=%d' count exceeding threshold of %d on host '%s'",
             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), label.c_str(), count, counter_analyzer->get_threshold(),
             host.c_str());
    ON_NULL_RETURN(message);
    OPAL_OUTPUT_VERBOSE((1, orcm_analytics_base_framework.framework_output, message));

    orcm_ras_event_t* event = NULL;
    do {
        // Add event to output data
        orcm_value_t* value = orcm_util_load_orcm_value((char*)label.c_str(), (void*)&count, OPAL_UINT32, NULL);
        ON_NULL_BREAK(value);
        opal_list_append(threshold_list, (opal_list_item_t*)value);

        // Handle store type here...
        if(true == step_data->store_event) {
            event = orcm_analytics_base_event_create(caddy->analytics_value, ORCM_RAS_EVENT_SENSOR, step_data->severity);
            ON_NULL_BREAK(event);
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_category(event, step_data->fault_type));
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_storage(event, ORCM_STORAGE_TYPE_DATABASE));
            ORCM_RAS_EVENT(event);
        }

        // Handle notification type here...
        if(true == step_data->use_notifier) {
            event = orcm_analytics_base_event_create(caddy->analytics_value, ORCM_RAS_EVENT_SENSOR, step_data->severity);
            ON_NULL_BREAK(event);
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_category(event, step_data->fault_type));
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_storage(event, ORCM_STORAGE_TYPE_NOTIFICATION));
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_description(event, (char*)"notifier_msg", (void*)message,
                                                                       OPAL_STRING, NULL));
            ON_FAILURE_BREAK(orcm_analytics_base_event_set_description(event, (char*)"notifier_action",
                                                                       (void*)step_data->notifier_action.c_str(),
                                                                       OPAL_STRING, NULL));
            ORCM_RAS_EVENT(event);
        }

        SAFE_FREE(message);
        return;
    } while(false);
    SAFE_OBJ_RELEASE(event);
    SAFE_FREE(message);
}

string get_hostname_from_caddy(orcm_workflow_caddy_t* caddy)
{
    string hostname;
    orcm_value_t* item = NULL;
    OPAL_LIST_FOREACH(item, caddy->analytics_value->key, orcm_value_t) {
        string key = item->value.key;
        if("hostname" == key) {
            hostname = item->value.data.string;
            break;
        }
    }
    return hostname;
}

time_t get_timeval_from_caddy(orcm_workflow_caddy_t* caddy)
{
    time_t tv = 0;
    orcm_value_t* item = NULL;
    OPAL_LIST_FOREACH(item, caddy->analytics_value->non_compute_data, orcm_value_t) {
        string key = item->value.key;
        if("ctime" == key) {
            tv = item->value.data.tv.tv_sec;
            break;
        }
    }
    return tv;
}

void add_data_values(const string& hostname, time_t tv, orcm_workflow_caddy_t* caddy, opal_list_t* threshold_list)
{
    analyze_data_t data;
    data.caddy = caddy;
    data.threshold_data = threshold_list;
    orcm_value_t* item = NULL;
    OPAL_LIST_FOREACH(item, caddy->analytics_value->compute_data, orcm_value_t) {
        if(true == counter_analyzer->is_wanted_label(item->value.key)) {
            counter_analyzer->add_value(hostname, item->value.key, (uint32_t)item->value.data.int32,
                                        tv, cott_event_fired_callback, &data);
        }
    }
}

int assert_caddy_data(void *cbdata)
{
    return orcm_analytics_base_assert_caddy_data(cbdata);
}

/* Sample workflow for this plugin (values set and used in the following function)
===================================================================================
nodelist:cn[3:1-254]
component:filter:args:data_group=errcounts
component:cott:args:severity=warn;fault_type=soft;store_event=yes;\
                    notifier_action=email;\
                    label_mask=CPU_SrcID#*_Channel#*_DIMM#*_CE;\
                    time_window=60s;count_threshold=10
===================================================================================
 * label_mask       is required and specified the data labels (key) to match using
 *                  wildcard syntax.
 * count_threshold  is required and denotes the number of new item counts within
 *                  the time_window which after the the event is fired (inclusive).
 * severity         is optional and defaults to "error" and can only be "emerg",
 *                  "alert", "crit", "error", "warn".
 * fault_type       is optional and defaults to "hard" and can only be "hard" or
 *                  "soft".
 * store_event      is optional and defaults to "yes" and can only be "yes" or "no".
 * notifier_action  is optional and defaults to "none" and can only be "none",
 *                  "email", "syslog".
 * time_window      is optional and defaults to 1 second; the format is a number
 *                  followed by a character denoting the unit of the number:
 *                  's' - seconds
 *                  'm' - minutes
 *                  'h' - hours
 *                  'd' - days
 */
void set_attributes(orcm_workflow_caddy_t* caddy)
{
    opal_list_t* wf_vars = &(caddy->wf_step->attributes);
    opal_value_t* item;
    OPAL_LIST_FOREACH(item, wf_vars, opal_value_t) {
        string key = item->key;
        if("label_mask" == key) {
            string mask = item->data.string;
            counter_analyzer->set_value_label_mask(mask);
        } else if("time_window" == key) {
            string window = item->data.string;
            counter_analyzer->set_window_size(window);
        } else if("count_threshold" == key) {
            string sthreshold = item->data.string;
            uint32_t threshold = 1; // If bad count_threshold string default to 1.
            if(10 >= sthreshold.size()) {
                int64_t val = atoll(sthreshold.c_str());
                if(val < (int64_t)0x100000000 && val > 0) {
                    threshold = (uint32_t)val;
                }
            }
            counter_analyzer->set_threshold(threshold);
        } else if("fault_type" == key) {
            string fault = item->data.string;
            if(lookup_fault.end() != lookup_fault.find(fault)) {
                global_step_data.fault_type = (int)lookup_fault[fault];
            }
        } else if("store_event" == key) {
            string sstore = item->data.string;
            if(lookup_store.end() != lookup_store.find(sstore)) {
                global_step_data.store_event = (int)lookup_store[sstore];
            }
        } else if("notifier_action" == key) {
            string action = item->data.string;
            if("none" == action) {
                global_step_data.use_notifier = false;
                global_step_data.notifier_action = action;
            } else {
                global_step_data.use_notifier = true;
                global_step_data.notifier_action = action;
            }
        } else if("severity" == key) {
            string severity = item->data.string;
            if(lookup_severity.end() != lookup_severity.find(severity)) {
                global_step_data.severity = lookup_severity[severity];
            }
        }
    }
}

int cott_analyze(int sd, short args, void *cbdata)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t* current_caddy = (orcm_workflow_caddy_t*)cbdata;

    do {
        if (ORCM_SUCCESS != (rc = assert_caddy_data_hook(cbdata))) {
            break;
        }

        set_attributes(current_caddy);

        string hostname = get_hostname_from_caddy(current_caddy);

        time_t tv = get_timeval_from_caddy(current_caddy);

        if(true == hostname.empty() || (time_t)0 == tv) {
            rc = ORCM_ERROR;
            break;
        }

        opal_list_t* threshold_list = OBJ_NEW(opal_list_t);
        ON_NULL_ALLOC_BREAK(threshold_list, rc);
        add_data_values(hostname, tv, current_caddy, threshold_list);
        OBJ_RELEASE(current_caddy->analytics_value->compute_data);
        current_caddy->analytics_value->compute_data = threshold_list;
        OBJ_RETAIN(current_caddy->analytics_value);
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                         current_caddy->hash_key, current_caddy->analytics_value);
    } while(false);
    SAFE_OBJ_RELEASE(current_caddy);
    return rc;
}

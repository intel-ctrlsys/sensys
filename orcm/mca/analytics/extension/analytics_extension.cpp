/*
 * Copyright (c) 2016     Intel, Inc. All rights reserved.
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

#include "analytics_extension.h"
#include "opal/util/output.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/notifier/notifier.h"
#include "orcm/mca/dispatch/dispatch.h"
#include "orcm/mca/analytics/base/analytics_private.h"
#include "orcm/mca/analytics/analytics_interface.h"
#include "orcm/mca/analytics/base/analytics_factory.h"
#include <time.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <list>


#define SAFE_FREE(x) if(NULL != x) { free((void*)x); x=NULL; }
#define SAFE_DELETE(x) { delete x; x=NULL; }
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) OBJ_RELEASE(x)
#define CHECK_NULL_ALLOC(x, e, label)  if(NULL==x) { e=ORCM_ERR_OUT_OF_RESOURCE; goto label; }
int extension_init(orcm_analytics_base_module_t *imod);
void extension_finalize(orcm_analytics_base_module_t *imod);
int extension_analyze(int sd, short args, void *cbdata);


extern "C" {
    mca_analytics_extension_module_t orcm_analytics_extension_module = {
        {
            extension_init,
            extension_finalize,
            extension_analyze,
            NULL,
            NULL
        },
    };

    extern orcm_value_t* orcm_util_load_orcm_value(char*, void*, int, char*);
    extern double orcm_util_get_number_orcm_value(orcm_value_t*);
    extern orcm_analytics_value_t* orcm_util_load_analytics_time_compute(
                                   opal_list_t*, opal_list_t*, opal_list_t*);
}

int extension_init(orcm_analytics_base_module_t *imod)
{
    if (NULL == imod) {
        return ORCM_ERR_BAD_PARAM;
    } else {
        return ORCM_SUCCESS;
    }
}

void extension_finalize(orcm_analytics_base_module_t *imod)
{
    mca_analytics_extension_module_t* mod = (mca_analytics_extension_module_t *)imod;
    SAFE_FREE(mod);
}


opal_list_t* convert_to_opal_list(dataContainer& result){

    opal_list_t* compute = OBJ_NEW(opal_list_t);
    dataContainer::iterator i = result.begin();
    while(i != result.end()){
        std::string units = result.getUnits(result.getKey(i));
        double item_value = result.getValue<double>(i);
        orcm_value_t* val = orcm_util_load_orcm_value((char*)(result.getKey(i)).c_str(),(void*)&item_value,OPAL_DOUBLE,(char*)units.c_str());
        if(NULL != val){
            opal_list_append(compute, (opal_list_item_t*)val);
        }
        i++;
    }
    return compute;
}

dataContainer create_compute_container(opal_list_t* compute_list)
{
    dataContainer compute_container;
    orcm_value_t* current_val = NULL;
    OPAL_LIST_FOREACH(current_val, compute_list, orcm_value_t){
        std::string key = current_val->value.key;
        std::string units = current_val->units;
        double val = orcm_util_get_number_orcm_value(current_val);
        compute_container.put<double>(key, val, units);
    }
    return compute_container;
}

dataContainer create_data_container(opal_list_t* datalist)
{
    dataContainer dc;
    orcm_value_t* current_val = NULL;
    OPAL_LIST_FOREACH(current_val, datalist, orcm_value_t){
        std::string key = current_val->value.key;
        if(key != "ctime"){
            std::string data = current_val->value.data.string;
            dc.put<std::string>(key, data, "");
        }
        else {
            struct timeval tv = current_val->value.data.tv;
            dc.put<timeval>("ctime", tv, "");
         }
    }
    return dc;
}

opal_list_t* convert_to_event_list(std::list<Event>& events, orcm_workflow_caddy_t* current_caddy, char* event_key)
{
    opal_list_t* event_list = OBJ_NEW(opal_list_t);
    if(events.empty()){
        return NULL;
    }
    std::list<Event>::iterator i = events.begin();
    while(i != events.end()){
       Event ev = *i;
       char* sev1 = (char*)(ev.severity.c_str());
       char* a1 = (char*)(ev.action.c_str());
       char* msg = (char*)(ev.msg.c_str());
       char* units = (char*)(ev.unit.c_str());
       int sev = orcm_analytics_event_get_severity(sev1);
       orcm_value_t* val = orcm_util_load_orcm_value(event_key,(void*)&ev.value,OPAL_DOUBLE,units);
       orcm_analytics_base_gen_notifier_event(val, current_caddy, sev, msg,
                                                      a1, event_list);
       i++;
    }
    return event_list;
}

void create_dataset(DataSet& ds, orcm_workflow_caddy_t* current_caddy) {
    ds.compute = create_compute_container(
            current_caddy->analytics_value->compute_data);
    ds.non_compute = create_data_container(
            current_caddy->analytics_value->non_compute_data);
    ds.key = create_data_container(current_caddy->analytics_value->key);
    ds.attributes = create_data_container(&current_caddy->wf_step->attributes);
}

int extension_analyze(int sd, short args, void* cbdata)
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t* current_caddy = (orcm_workflow_caddy_t*)cbdata;
    orcm_analytics_value_t* analytics_value_to_next = NULL;
    Analytics* analytics = NULL;
    char* event_key = NULL;
    opal_list_t* event_list = NULL;
    if (ORCM_SUCCESS != (rc = orcm_analytics_base_assert_caddy_data(cbdata))) {
        SAFE_OBJ_RELEASE(current_caddy);
        return ORCM_ERR_BAD_PARAM;
    }
    char* plugin_name = current_caddy->wf_step->analytic;
    dataContainer compute_result;
    std::list<Event> events;
    DataSet ds(compute_result, events);
    create_dataset(ds, current_caddy);
    if(NULL == current_caddy->imod->orcm_mca_analytics_data_store)
    {
       AnalyticsFactory* factory = AnalyticsFactory::getInstance();
       analytics = factory->createPlugin((const char*)(current_caddy->wf_step->analytic));
       if(NULL == analytics){
           SAFE_OBJ_RELEASE(current_caddy);
           return ORCM_ERR_OUT_OF_RESOURCE;
       }
       current_caddy->imod->orcm_mca_analytics_data_store = analytics;
    } else {
       analytics = (Analytics*) current_caddy->imod->orcm_mca_analytics_data_store;
    }
    analytics->analyze(ds);
    opal_list_t* compute_list = convert_to_opal_list(ds.results);
    orcm_analytics_base_data_key(&event_key, plugin_name, current_caddy->wf, current_caddy->wf_step);
    CHECK_NULL_ALLOC(event_key, rc, cleanup);
    event_list = convert_to_event_list(ds.events, current_caddy, event_key);
    analytics_value_to_next = orcm_util_load_analytics_time_compute(current_caddy->analytics_value->key,
                                                  current_caddy->analytics_value->non_compute_data, compute_list);
    if(NULL != analytics_value_to_next){
        if(true == orcm_analytics_base_db_check(current_caddy->wf_step)){
            rc = orcm_analytics_base_log_to_database_event(analytics_value_to_next);
            if(ORCM_SUCCESS != rc){
                goto cleanup;
            }
        }
        ORCM_ACTIVATE_NEXT_WORKFLOW_STEP(current_caddy->wf, current_caddy->wf_step,
                                         current_caddy->hash_key, analytics_value_to_next, event_list);
    } else {
        rc = ORCM_ERR_OUT_OF_RESOURCE;
    }
cleanup:
    if(ORCM_SUCCESS != rc){
        SAFE_OBJ_RELEASE(compute_list);
        SAFE_OBJ_RELEASE(event_list);
    }
    SAFE_FREE(event_key);
    SAFE_OBJ_RELEASE(current_caddy);
    return rc;
}



/*Copyright (c) 2015      Intel, Inc. All rights reserved.*/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "orcm/mca/analytics/aggregate/analytics_aggregate.h"
extern mca_analytics_aggregate_module_t orcm_analytics_aggregate_module;

int analytics_aggregate_init_null_input_test(void);
int analytics_aggregate_analyze_null_input_test(void);
int analytics_aggregate_analyze_null_data_test(void);
int analytics_aggregate_finalize_test(void);
int analytics_aggregate_analyze_test_min(void);
int analytics_aggregate_analyze_test_max(void);
int analytics_aggregate_analyze_test_average(void);
int analytics_aggregate_analyze_incorrect_attribute_test(void);
int analytics_aggregate_analyze_incorrect_operation_test(void);

int analytics_aggregate_init_null_input_test() {
    int rc = ORCM_SUCCESS;
    rc = orcm_analytics_aggregate_module.api.init(NULL);
    if (ORCM_ERR_BAD_PARAM == rc) {
        return ORCM_SUCCESS;
    } else {
        return ORCM_ERROR;
    }
}

int analytics_aggregate_analyze_null_input_test() {
    int rc = ORCM_SUCCESS;
    rc = orcm_analytics_aggregate_module.api.analyze(0, 0, NULL);
    if (ORCM_ERR_BAD_PARAM == rc) {
        return ORCM_SUCCESS;
    } else {
        return ORCM_ERROR;
    }
}

int analytics_aggregate_analyze_incorrect_attribute_test()
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    temp->key = strdup("incorrectkey");
    temp->data.string = strdup("average");

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);

    rc = orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
    if (ORCM_SUCCESS != rc) {
        return ORCM_SUCCESS;
    } else {
        return ORCM_ERROR;
    }
}

int analytics_aggregate_analyze_null_data_test()
{
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    temp->key = strdup("operation");
    temp->data.string = strdup("average");
    opal_list_t* computelist = OBJ_NEW(opal_list_t);

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);

    caddy->analytics_value->compute_data = computelist;

    return orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
}

int analytics_aggregate_analyze_incorrect_operation_test()
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    orcm_value_t* compute_data = OBJ_NEW(orcm_value_t);
    temp->key = strdup("operation");
    temp->data.string = strdup("abcde");

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->analytics_value->compute_data = OBJ_NEW(opal_list_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);
    opal_list_append(caddy->analytics_value->compute_data,(opal_list_item_t*) compute_data);
    rc = orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
    if (ORCM_ERR_BAD_PARAM == rc) {
        return ORCM_SUCCESS;
    } else {
        return ORCM_ERROR;
    }
}

int analytics_aggregate_finalize_test() {
    int rc = ORCM_SUCCESS;
    orcm_analytics_aggregate_module.api.finalize(NULL);
    return rc;
}

int analytics_aggregate_analyze_test_average()
{
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    orcm_value_t* compute_data = OBJ_NEW(orcm_value_t);
    temp->key = strdup("operation");
    temp->data.string = strdup("average");
    opal_list_t* computelist = OBJ_NEW(opal_list_t);

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    memset(caddy->imod,0,sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);
    compute_data->units = strdup("Celcius");
    compute_data->value.data.dval = 35.0;
    opal_list_append(computelist,(opal_list_item_t*) compute_data);
    caddy->analytics_value->compute_data = computelist;

    rc = orcm_analytics_aggregate_module.api.init(caddy->imod);
    if(rc == ORCM_SUCCESS){
        rc = orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
    }
    return rc;
}

int analytics_aggregate_analyze_test_min() {
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    orcm_value_t* compute_data = OBJ_NEW(orcm_value_t);
    temp->key = strdup("operation");
    temp->data.string = strdup("min");
    opal_list_t* computelist = OBJ_NEW(opal_list_t);

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    memset(caddy->imod,0,sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);
    compute_data->units = strdup("Celcuis");
    compute_data->value.data.dval = 35.0;
    opal_list_append(computelist,(opal_list_item_t*) compute_data);
    caddy->analytics_value->compute_data = computelist;

    rc = orcm_analytics_aggregate_module.api.init(caddy->imod);
    if(rc == ORCM_SUCCESS){
        rc = orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
    }
    return rc;
}

int analytics_aggregate_analyze_test_max() {
    int rc = ORCM_SUCCESS;
    orcm_workflow_caddy_t *caddy = OBJ_NEW(orcm_workflow_caddy_t);
    opal_value_t* temp = OBJ_NEW(opal_value_t);
    orcm_value_t* compute_data = OBJ_NEW(orcm_value_t);
    temp->key = strdup("operation");
    temp->data.string = strdup("max");
    opal_list_t* computelist = OBJ_NEW(opal_list_t);

    caddy->wf_step = OBJ_NEW(orcm_workflow_step_t);
    caddy->wf = OBJ_NEW(orcm_workflow_t);
    caddy->wf->workflow_id = 0;
    caddy->analytics_value = OBJ_NEW(orcm_analytics_value_t);
    caddy->imod = (orcm_analytics_base_module_t*)malloc(sizeof(struct orcm_analytics_base_module));
    memset(caddy->imod,0,sizeof(struct orcm_analytics_base_module));
    opal_list_append(&caddy->wf_step->attributes,(opal_list_item_t*)temp);
    compute_data->units = strdup("Celcius");
    compute_data->value.data.dval = 35.0;
    opal_list_append(computelist,(opal_list_item_t*) compute_data);
    caddy->analytics_value->compute_data = computelist;

    rc = orcm_analytics_aggregate_module.api.init(caddy->imod);
    if(rc == ORCM_SUCCESS){
        rc = orcm_analytics_aggregate_module.api.analyze(0, 0, (void*)caddy);
    }
    return rc;
}

int main(int argc, char *argv[]) {
    int rc = ORCM_SUCCESS;

    if(NULL == argv[1]){
        return 1;
    }
    if (0 == strcmp(argv[1], "init")) {
        rc = analytics_aggregate_init_null_input_test();
    }
    else if (0 == strcmp(argv[1], "finalize")) {
        rc = analytics_aggregate_finalize_test();
    }
    else if (0 == strcmp(argv[1], "analyze_null_input")) {
        rc = analytics_aggregate_analyze_null_input_test();
    }
    else if (0 == strcmp(argv[1], "analyze_incorrect_attribute")) {
        rc = analytics_aggregate_analyze_incorrect_attribute_test();
    }
    else if (0 == strcmp(argv[1], "incorrect_operation")) {
        rc = analytics_aggregate_analyze_incorrect_operation_test();
    }
    else if (0 == strcmp(argv[1], "analyze_null_data")) {
        rc = analytics_aggregate_analyze_null_data_test();
    }
    else if (0 == strcmp(argv[1], "min")) {
        rc = analytics_aggregate_analyze_test_min();
    }
    else if (0 == strcmp(argv[1], "max")) {
        rc = analytics_aggregate_analyze_test_max();
    }
    else if (0 == strcmp(argv[1], "average")) {
        rc = analytics_aggregate_analyze_test_average();
    }
    if (ORCM_SUCCESS == rc) {
        return 0;
    } else {
        return 1;
    }
}

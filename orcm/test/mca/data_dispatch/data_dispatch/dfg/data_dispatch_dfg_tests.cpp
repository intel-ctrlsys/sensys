/* * Copyright (c) 2015      Intel Corporation. All rights reserved.*/

#include "data_dispatch_dfg_tests.h"
#include <stdio.h>
#include <stdbool.h>

extern orcm_db_API_module_t orcm_db;


void orcm_data_dispatch_tests_cleanup(void *cbdata)
{
    orcm_ras_event_t *analytics_event_data;

    if (NULL == cbdata) {
        return;
    }

    analytics_event_data = (orcm_ras_event_t *)cbdata;
    OBJ_RELEASE(analytics_event_data);
}

void orcm_data_dispatch_test_open(char *name, opal_list_t *properties,
                              orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    opal_list_t *input_list;

    input_list = OBJ_NEW(opal_list_t);

    if (NULL == input_list) {
        return;
    }

    if (NULL != cbfunc) {
        cbfunc(1, 1, input_list, NULL, NULL);
    }
    if (NULL != cbfunc) {
        cbfunc(1, 0, NULL, NULL, NULL);
    }
}

void orcm_data_dispatch_test_close(int dbhandle, orcm_db_callback_fn_t cbfunc, void *cbdata)
{
    opal_list_t *input_list;

    input_list = OBJ_NEW(opal_list_t);

    if (NULL == input_list) {
        return;
    }

    if (NULL != cbfunc) {
        cbfunc(1, 1, input_list, NULL, NULL);
    }
    if (NULL != cbfunc) {
        cbfunc(1, 0, NULL, NULL, NULL);
    }

}

void orcm_data_dispatch_test_store_new(int dbhandle, orcm_db_data_type_t data_type, opal_list_t *input,
                                   opal_list_t *ret, orcm_db_callback_fn_t cbfunc, void *cbdata)
{

    if (NULL != cbfunc) {
        cbfunc(1, 0, NULL, NULL, NULL);
    }
    if (NULL != cbfunc) {
        cbfunc(1, 0, input, NULL, NULL);
    }

}

void orcm_data_dispatch_test_setup()
{
    orcm_db.open =  orcm_data_dispatch_test_open;
    orcm_db.close = orcm_data_dispatch_test_close;
    orcm_db.store_new = orcm_data_dispatch_test_store_new;
}

void orcm_data_dispatch_test_tear_down()
{
    orcm_db.open =  orcm_db_base_open;
    orcm_db.close = orcm_db_base_close;
    orcm_db.store_new = orcm_db_base_store_new;
}


TEST(data_dispatch_dfg, init_void)
{
    orcm_data_dispatch_test_setup();
    orcm_data_dispatch_dfg_module.init();
    orcm_data_dispatch_test_tear_down();
}

TEST(data_dispatch_dfg, init_env_set_commit_void)
{
    orcm_data_dispatch_test_setup();
    orcm_data_dispatch_base.sensor_db_commit_rate = 200;
    orcm_data_dispatch_dfg_module.init();
    orcm_data_dispatch_base.sensor_db_commit_rate = 1;
    orcm_data_dispatch_test_tear_down();
}

TEST(data_dispatch_dfg, finalize_void)
{
    orcm_data_dispatch_test_setup();
    orcm_data_dispatch_dfg_module.finalize();
    orcm_data_dispatch_test_tear_down();
}

TEST(data_dispatch_dfg, init_finalize_void)
{
    orcm_data_dispatch_test_setup();
    orcm_data_dispatch_dfg_module.init();
    orcm_data_dispatch_dfg_module.finalize();
    orcm_data_dispatch_test_tear_down();
}

TEST(data_dispatch_dfg, generate_NULL_ecd)
{
    orcm_data_dispatch_test_setup();
    orcm_data_dispatch_dfg_module.init();
    orcm_data_dispatch_dfg_module.generate(NULL);
    orcm_data_dispatch_dfg_module.finalize();
    orcm_data_dispatch_test_tear_down();
}


TEST(data_dispatch_dfg, generate_no_type_severity)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_no_severity)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_no_type)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_with_severity_type)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_sensor_db_event_without_init)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        ecd->cbfunc = orcm_data_dispatch_tests_cleanup;
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_success_sensor_db_event)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->cbfunc = orcm_data_dispatch_tests_cleanup;
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_success_sensor_db_event_with_commit_rate)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_base.sensor_db_commit_rate = 200;
        orcm_data_dispatch_dfg_module.init();
        ecd->cbfunc = orcm_data_dispatch_tests_cleanup;
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_base.sensor_db_commit_rate = 1;
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_non_sensor_db_event_without_init)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        ecd->cbfunc = orcm_data_dispatch_tests_cleanup;
        ecd->type = ORCM_RAS_EVENT_SENSOR;
        ecd->severity = ORCM_RAS_SEVERITY_INFO;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_success_non_sensor_db_event)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->cbfunc = orcm_data_dispatch_tests_cleanup;
        ecd->type = ORCM_RAS_EVENT_EXCEPTION;
        ecd->severity = ORCM_RAS_SEVERITY_CRIT;
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_empty_description)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        OBJ_DESTRUCT(&ecd->description);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_description_empty_item)
{
    orcm_value_t *item = OBJ_NEW(orcm_value_t);

    if (NULL == item) {
        return;
    }

    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        opal_list_append(&ecd->description, (opal_list_item_t *)item);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_notification_null_msg_null_action)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_NOTIFICATION);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_notification_empty_item)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    orcm_value_t *item = OBJ_NEW(orcm_value_t);

    if (NULL == item) {
        return;
    }

    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_NOTIFICATION);
        opal_list_append(&ecd->description, (opal_list_item_t *)item);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_notification_empty_action)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_NOTIFICATION);
        orcm_analytics_base_event_set_description(ecd, (char *)"notifier_msg", (void *)"threshold is high", OPAL_STRING, NULL);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_notification_unknown_severity)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_NOTIFICATION);
        orcm_analytics_base_event_set_description(ecd, (char *)"notifier_msg", (void *)"threshold is high", OPAL_STRING, NULL);
        orcm_analytics_base_event_set_description(ecd, (char *)"notifier_action", (void *)"high", OPAL_STRING, NULL);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_notification_negative_severity)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        ecd->severity = -1;
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_NOTIFICATION);
        orcm_analytics_base_event_set_description(ecd, (char *)"notifier_msg", (void *)"threshold is high", OPAL_STRING, NULL);
        orcm_analytics_base_event_set_description(ecd, (char *)"notifier_action", (void *)"high", OPAL_STRING, NULL);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_pubsub)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_PUBSUB);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}

TEST(data_dispatch_dfg, generate_storage_undefined)
{
    orcm_ras_event_t *ecd = OBJ_NEW(orcm_ras_event_t);
    if (NULL != ecd) {
        orcm_data_dispatch_test_setup();
        orcm_data_dispatch_dfg_module.init();
        orcm_analytics_base_event_set_storage(ecd, ORCM_STORAGE_TYPE_UNDEFINED);
        orcm_data_dispatch_dfg_module.generate(ecd);
        orcm_data_dispatch_dfg_module.finalize();
        orcm_data_dispatch_test_tear_down();
    }
}


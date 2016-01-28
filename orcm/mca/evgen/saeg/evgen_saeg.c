/*
 * Copyright (c) 2015 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/argv.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"

#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/notifier/base/base.h"
#include "orte/mca/notifier/notifier.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"
#include "orcm/mca/evgen/base/base.h"
#include "orcm/mca/evgen/saeg/evgen_saeg.h"

/* API functions */

static void saeg_init(void);
static void saeg_finalize(void);
static void saeg_generate(orcm_ras_event_t *cd);

/* The module struct */

orcm_evgen_base_module_t orcm_evgen_saeg_module = {
    saeg_init,
    saeg_finalize,
    saeg_generate
};

/* local variables */
static int env_dbhandle = -1;
static int event_dbhandle = -1;
static int env_db_commit_count = 0;

static void saeg_env_db_open_cbfunc(int dbh, int status, opal_list_t *input_list, opal_list_t *output_list,
                                    void *cbdata)
{
    if (0 == status) {
        env_dbhandle = dbh;
    } else {
        OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                             "%s evgen:saeg DB env open operation failed",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    }

    if (NULL != input_list) {
        OBJ_RELEASE(input_list);
    }

}

static void saeg_event_db_open_cbfunc(int dbh, int status, opal_list_t *input_list, opal_list_t *output_list,
                                      void *cbdata)
{
    if (0 == status) {
        event_dbhandle = dbh;
    } else {
        OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                             "%s evgen:saeg DB event open operation failed",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    }

    if (NULL != input_list) {
        OBJ_RELEASE(input_list);
    }

}

static void saeg_data_cbfunc(int dbh, int status, opal_list_t *input_list, opal_list_t *output_list,
                             void *cbdata)
{
    if (NULL != input_list) {
        OBJ_RELEASE(input_list);
    }

}

static opal_list_t* saeg_init_env_dbhandle_commit_rate(void)
{
    orcm_value_t *attribute;
    opal_list_t *props = NULL; /* DB Attributes list */

    props = OBJ_NEW(opal_list_t);

    if (NULL != props) {
        attribute = OBJ_NEW(orcm_value_t);
        if (NULL != attribute) {
            attribute->value.key = strdup("autocommit");
            attribute->value.type = OPAL_BOOL;
            if (orcm_evgen_base.sensor_db_commit_rate > 1) {
                attribute->value.data.flag = false; /* Disable Auto commit/Enable grouped commits */
            } else {
                attribute->value.data.flag = true; /* Enable Auto commit/Disable grouped commits */
            }
            opal_list_append(props, (opal_list_item_t *)attribute);
        }
        else {
            OBJ_RELEASE(props);
        }
    }
    return props;
}

static void saeg_init(void)
{
    opal_list_t *props = NULL;

    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:saeg init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    if (0 > env_dbhandle) {
        /* get a db handle for env data*/
        props = saeg_init_env_dbhandle_commit_rate();
        orcm_db.open("saeg_env", props, saeg_env_db_open_cbfunc, NULL);
    }

    if (0 > event_dbhandle) {
        /* get a db handle for event data*/
        orcm_db.open("saeg_event", NULL, saeg_event_db_open_cbfunc, NULL);
    }

    return;
}

static void saeg_finalize(void)
{

    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:saeg finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (0 <= env_dbhandle) {
        orcm_db.close(env_dbhandle, NULL, NULL);
        env_dbhandle = -1;
    }

    if (0 <= event_dbhandle) {
        orcm_db.close(event_dbhandle, NULL, NULL);
        event_dbhandle = -1;
    }
    return;
}

static opal_list_t* saeg_convert_event_data_to_list(orcm_ras_event_t *ecd)
{
    orcm_value_t *metric = NULL;
    struct timeval eventtime;
    opal_list_t *input_list = NULL;

    input_list = OBJ_NEW(opal_list_t);
    if (NULL == input_list) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return NULL;
    }

    metric = orcm_util_load_orcm_value("type", (void *) orcm_evgen_base_print_type(ecd->type), OPAL_STRING, NULL);
    if (NULL == metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(input_list);
        return NULL;
    }
    opal_list_append(input_list, (opal_list_item_t *)metric);

    metric = orcm_util_load_orcm_value("severity", (void *) orcm_evgen_base_print_severity(ecd->severity), OPAL_STRING, NULL);
    if (NULL == metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(input_list);
        return NULL;
    }
    opal_list_append(input_list, (opal_list_item_t *)metric);

    eventtime.tv_sec = ecd->timestamp;
    eventtime.tv_usec = 0L;

    metric = orcm_util_load_orcm_value("ctime", &eventtime, OPAL_TIMEVAL, NULL);
    if (NULL == metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(input_list);
        return NULL;
    }
    opal_list_append(input_list, (opal_list_item_t *)metric);

    /*This is an O(1) operation. But contents of list are permanently transferred  */
    opal_list_join(input_list, (opal_list_item_t *)metric, &ecd->reporter);

    opal_list_join(input_list, (opal_list_item_t *)metric, &ecd->description);

    opal_list_join(input_list, (opal_list_item_t *)metric, &ecd->data);

    return input_list;
}

static void saeg_env_data_commit_cb(int dbhandle, int status, opal_list_t *in,
                                    opal_list_t *out, void *cbdata) {
    if (ORCM_SUCCESS != status) {
        ORTE_ERROR_LOG(status);
        orcm_db.rollback(dbhandle, NULL, NULL);
    }
}


static void saeg_generate_database_event(opal_list_t *input_list, int data_type)
{

    if (ORCM_RAS_EVENT_SENSOR == data_type ) {
        if (0 <= env_dbhandle) {
            orcm_db.store_new(env_dbhandle, ORCM_DB_ENV_DATA, input_list, NULL, saeg_data_cbfunc, NULL);
            if (orcm_evgen_base.sensor_db_commit_rate > 1) {
                env_db_commit_count++;
                if (env_db_commit_count == orcm_evgen_base.sensor_db_commit_rate) {
                    orcm_db.commit(env_dbhandle, saeg_env_data_commit_cb, NULL);
                    env_db_commit_count = 0;
                }
            }
        }
        else {
            OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                                 "%s evgen:saeg couldn't store env data as db handler isn't opened",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        }
    }
    else {
        if (0 <= event_dbhandle) {
            orcm_db.store_new(event_dbhandle, ORCM_DB_EVENT_DATA, input_list, NULL, saeg_data_cbfunc, NULL);
        }
        else {
            OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                                 "%s evgen:saeg couldn't store event data as db handler isn't opened",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        }
    }
}
static void saeg_generate_notifier_event(orcm_ras_event_t *ecd)
{
    orcm_value_t *list_item = NULL;
    char *notifier_msg = NULL;
    char *notifier_action = NULL;

    OPAL_LIST_FOREACH(list_item, &ecd->description, orcm_value_t) {
        if (NULL == list_item || NULL == list_item->value.key) {
            SAFEFREE(notifier_msg);
            SAFEFREE(notifier_action);
        }
        if (0 == strcmp(list_item->value.key, "notifier_msg") &&
            NULL != list_item->value.data.string && NULL == notifier_msg) {
            notifier_msg = strdup(list_item->value.data.string);
        }
        if (0 == strcmp(list_item->value.key, "notifier_action") &&
            NULL != list_item->value.data.string && NULL == notifier_action) {
            notifier_action = strdup(list_item->value.data.string);
        }
    }
    if ((NULL != notifier_msg) && (NULL != notifier_action)) {
       if ((ecd->severity >= ORCM_RAS_SEVERITY_EMERG) && (ecd->severity < ORCM_RAS_SEVERITY_UNKNOWN)) {
           OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                                "%s evgen:saeg generating notifier with severity %d "
                                "and notifier action as %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ecd->severity, notifier_action));
            ORTE_NOTIFIER_SYSTEM_EVENT(ecd->severity, notifier_msg, notifier_action);
        }
    }
}

static void saeg_generate_storage_events(orcm_ras_event_t *ecd)
{
    opal_list_t *input_list = NULL;
    orcm_value_t *list_item = NULL;

    OPAL_LIST_FOREACH(list_item, &ecd->description, orcm_value_t) {
        if (NULL == list_item->value.key) {
            break;
        }
        if (0 == strcmp(list_item->value.key, "storage_type")) {
            switch (list_item->value.data.uint) {
            case ORCM_STORAGE_TYPE_NOTIFICATION: //Need implementation
                saeg_generate_notifier_event(ecd);
                break;

            case ORCM_STORAGE_TYPE_PUBSUB: //Need implementation
                break;

            default:
                break;
            }

        }

    }

    //Send event to DB regardless of storage_type key in event
    input_list = saeg_convert_event_data_to_list(ecd);

    if (NULL == input_list) {
        return;
    }

    saeg_generate_database_event(input_list, ecd->type);
}

static void saeg_generate(orcm_ras_event_t *ecd)
{

    OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                         "%s evgen:saeg record event",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (NULL == ecd) {
        OPAL_OUTPUT_VERBOSE((1, orcm_evgen_base_framework.framework_output,
                             "%s evgen:saeg NULL event data",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return;
    }

    saeg_generate_storage_events(ecd);

    return;
}

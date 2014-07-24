/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */

#include "opal_stdint.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_test.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void test_sample(orcm_sensor_sampler_t *sampler);
static void test_log(opal_buffer_t *buf);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_test_module = {
    init,
    finalize,
    start,
    stop,
    test_sample,
    test_log
};

static opal_list_t tracking;

static int init(void)
{
    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);


    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to read */
        orte_show_help("help-orcm-sensor-test.txt", "no-test-found",
                       true, orte_process_info.nodename);
        return ORTE_ERROR;
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_LIST_DESTRUCT(&tracking);
}

static void start(orte_jobid_t jobid)
{
    return;
}

static void stop(orte_jobid_t jobid)
{
    return;
}

static void test_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    opal_buffer_t data, *bptr;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    char *temp;
    bool packed;
    struct tm *sample_time;

    if (0 == opal_list_get_size(&tracking)) {
        return;
    }

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    temp = strdup("test");
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &temp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(temp);

    /* store our hostname */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* get the sample time */
    now = time(NULL);
    /* pass the time along as a simple string */
    sample_time = localtime(&now);
    if (NULL == sample_time) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    strftime(time_str, sizeof(time_str), "%F %T%z", sample_time);
    asprintf(&timestamp_str, "%s", time_str);
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &timestamp_str, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(ret);
        OBJ_DESTRUCT(&data);
        free(timestamp_str);
        return;
    }
    free(timestamp_str);

    /* xfer the data for transmission */
    if (packed) {
        bptr = &data;
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
    OBJ_DESTRUCT(&data);
}

static void test_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    int rc;
    int32_t n;

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received log from host %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname);

    if (NULL != hostname) {
        free(hostname);
    }

}

/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC.  All rights
 *                         reserved.
 * Copyright (c) 2014-2016  Intel Corporation.  All rights reserved.
 *
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

#include "opal_stdint.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_ring_buffer.h"
#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/mca/pstat/pstat.h"

#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/state/state.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/db/db.h"
#include "orcm/util/utils.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/analytics/analytics.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_resusage.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
void collect_resusage_sample(orcm_sensor_sampler_t *sampler);
static void res_log(opal_buffer_t *collect_resusage_sample);
static void res_inventory_collect(opal_buffer_t *inventory_snapshot);
static void res_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int resusage_enable_sampling(const char* sensor_specification);
int resusage_disable_sampling(const char* sensor_specification);
int resusage_reset_sampling(const char* sensor_specification);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_resusage_module = {
    init,
    finalize,
    NULL,
    NULL,
    collect_resusage_sample,
    res_log,
    res_inventory_collect,
    res_inventory_log,
    NULL,
    NULL,
    resusage_enable_sampling,
    resusage_disable_sampling,
    resusage_reset_sampling
};

static orte_node_t *my_node;
static orte_proc_t *my_proc;

static void generate_test_vector(opal_buffer_t *v);

static int init(void)
{
    orte_job_t *jdata;

    mca_sensor_resusage_component.diagnostics = 0;
    mca_sensor_resusage_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("resusage", orcm_sensor_base.collect_metrics,
                                                mca_sensor_resusage_component.collect_metrics);

    /* ensure my_proc and my_node are available on the global arrays */
    if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
        my_proc = OBJ_NEW(orte_proc_t);
        my_node = OBJ_NEW(orte_node_t);
    } else {
        if (NULL == (my_proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, ORTE_PROC_MY_NAME->vpid))) {
            ORTE_ERROR_LOG(ORCM_ERR_NOT_FOUND);
            return ORCM_ERR_NOT_FOUND;
        }
        if (NULL == (my_node = my_proc->node)) {
            ORTE_ERROR_LOG(ORCM_ERR_NOT_FOUND);
            return ORCM_ERR_NOT_FOUND;
        }
        /* protect the objects */
        OBJ_RETAIN(my_proc);
        OBJ_RETAIN(my_node);
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    if (NULL != my_proc) {
        OBJ_RELEASE(my_proc);
    }
    if (NULL != my_node) {
        OBJ_RELEASE(my_node);
    }

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_resusage_component.runtime_metrics);
    mca_sensor_resusage_component.runtime_metrics = NULL;
    return;
}

void collect_resusage_sample(orcm_sensor_sampler_t *sampler)
{
    opal_pstats_t *stats;
    opal_node_stats_t *nstats;
    int rc, i;
    orte_proc_t *child;
    opal_buffer_t buf, *bptr;
    const char *comp = "resusage";
    struct timeval current_time;
    void* metrics_obj = mca_sensor_resusage_component.runtime_metrics;

    if(!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor resusage : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_resusage_component.diagnostics |= 0x1;

    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "sample:resusage sampling resource usage"));
    if (mca_sensor_resusage_component.test) {
        /* generate and send a the test vector */
        OBJ_CONSTRUCT(&buf, opal_buffer_t);
        generate_test_vector(&buf);
        bptr = &buf;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        OBJ_DESTRUCT(&buf);
        return;
    }


    /* setup a buffer for our stats */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    /* pack our name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return;
    }

    /* update stats on ourself and the node */
    stats = OBJ_NEW(opal_pstats_t);
    nstats = OBJ_NEW(opal_node_stats_t);
    if (ORCM_SUCCESS != (rc = opal_pstat.query(orte_process_info.pid, stats, nstats))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        OBJ_DESTRUCT(&buf);
        return;
    }

    /* the stats framework can't know nodename or rank */
    strncpy(stats->node, orcm_sensor_base.host_tag_value, (OPAL_PSTAT_MAX_STRING_LEN - 1));
    stats->rank = ORTE_PROC_MY_NAME->vpid;

    /* pack them */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf,
                         &orcm_sensor_base.host_tag_value, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    /* get the sample time */
    gettimeofday(&current_time, NULL);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &current_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &nstats, 1, OPAL_NODE_STAT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &stats, 1, OPAL_PSTAT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    /* loop through our children and update their stats */
    if (NULL != orte_local_children) {
        for (i=0; i < orte_local_children->size; i++) {
            if (NULL == (child = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i))) {
                continue;
            }
            if (!ORTE_FLAG_TEST(child, ORTE_PROC_FLAG_ALIVE)) {
                continue;
            }
            if (0 == child->pid) {
                /* race condition */
                continue;
            }
            stats = OBJ_NEW(opal_pstats_t);
            if (ORCM_SUCCESS != opal_pstat.query(child->pid, stats, NULL)) {
                /* may hit a race condition where the process has
                 * terminated, so just ignore any error
                 */
                SAFE_RELEASE(stats);
                continue;
            }
            /* the stats framework can't know nodename or rank */
            strncpy(stats->node, orcm_sensor_base.host_tag_value, (OPAL_PSTAT_MAX_STRING_LEN - 1));
            stats->rank = child->name.vpid;
            /* pack them */
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &stats, 1, OPAL_PSTAT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&buf);
                SAFE_RELEASE(stats);
                SAFE_RELEASE(nstats);
                return;
            }
        }
    }

    /* xfer any data for transmission */
    if (0 < buf.bytes_used) {
        bptr = &buf;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&buf);
            SAFE_RELEASE(stats);
            SAFE_RELEASE(nstats);
            return;
        }
    }

    SAFE_RELEASE(stats);
    SAFE_RELEASE(nstats);
    OBJ_DESTRUCT(&buf);
}

static void res_log_sample_item(opal_list_t *key, opal_list_t *non_compute_data, char *sample_key,
                                void *sample_item, opal_data_type_t type, char *units )
{
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;

    analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
    if ((NULL == analytics_vals) || (NULL == analytics_vals->key) ||
         (NULL == analytics_vals->non_compute_data) ||(NULL == analytics_vals->compute_data)) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }

    sensor_metric = orcm_util_load_orcm_value(sample_key, sample_item, type, units);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }

    opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
    orcm_analytics.send_data(analytics_vals);

cleanup:
    if ( NULL != analytics_vals) {
        OBJ_RELEASE(analytics_vals);
    }

}

static void res_log_node_stats(opal_node_stats_t *nst, char *node, struct timeval sampletime)
{
    orcm_value_t *sensor_metric = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;

    key = OBJ_NEW(opal_list_t);
    if (NULL == key) {
        return;
    }

    non_compute_data = OBJ_NEW(opal_list_t);
    if (NULL == non_compute_data) {
        goto cleanup;
    }

    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("hostname", node, OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("data_group", "nodestats", OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    res_log_sample_item(key, non_compute_data, "total_mem", &nst->total_mem, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "free_mem", &nst->free_mem, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "buffers", &nst->buffers, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "cached", &nst->cached, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "swap_total", &nst->swap_total, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "swap_free", &nst->swap_free, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "mapped", &nst->mapped, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "swap_cached", &nst->swap_cached, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "la", &nst->la, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "la5", &nst->la5, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "la15", &nst->la15, OPAL_FLOAT, "MB");

cleanup:
    if ( NULL != key) {
        OBJ_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
        OBJ_RELEASE(non_compute_data);
    }

}

static void res_log_process_stats(opal_pstats_t *st, char *node, struct timeval sampletime)
{
    char *primary_key = NULL;
    orcm_value_t *sensor_metric = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;
    char state[3];

    key = OBJ_NEW(opal_list_t);
    if (NULL == key) {
        return;
    }

    non_compute_data = OBJ_NEW(opal_list_t);
    if (NULL == non_compute_data) {
        goto cleanup;
    }

    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("hostname", node, OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    asprintf(&primary_key, "procstat_%s",st->cmd);
    if (NULL == primary_key){
        goto cleanup;
    }

    sensor_metric = orcm_util_load_orcm_value("data_group", primary_key, OPAL_STRING, NULL);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    res_log_sample_item(key, non_compute_data, "rank", &st->rank, OPAL_INT32, NULL);

    res_log_sample_item(key, non_compute_data, "pid", &st->pid, OPAL_PID, NULL);

    res_log_sample_item(key, non_compute_data, "cmd", st->cmd, OPAL_STRING, NULL);

    if (0 > snprintf(state, sizeof(state), "%s", st->state)) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }
    res_log_sample_item(key, non_compute_data, "state", state, OPAL_STRING, NULL);

    res_log_sample_item(key, non_compute_data, "percent_cpu", &st->percent_cpu, OPAL_FLOAT, "%");

    res_log_sample_item(key, non_compute_data, "priority", &st->priority, OPAL_INT32, NULL);

    res_log_sample_item(key, non_compute_data, "num_threads", &st->num_threads, OPAL_INT16, NULL);

    res_log_sample_item(key, non_compute_data, "vsize", &st->vsize, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "rss", &st->rss, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "peak_vsize", &st->peak_vsize, OPAL_FLOAT, "MB");

    res_log_sample_item(key, non_compute_data, "processor", &st->processor, OPAL_INT16, NULL);

cleanup:
    SAFEFREE(primary_key);
    if ( NULL != key) {
        OBJ_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
        OBJ_RELEASE(non_compute_data);
    }
}

static void res_log(opal_buffer_t *collect_resusage_sample)
{
    opal_pstats_t *st=NULL;
    opal_node_stats_t *nst=NULL;
    int rc, n;
    char *node = NULL;
    struct timeval sampletime;


    /* unpack the node name */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(collect_resusage_sample, &node, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(collect_resusage_sample, &sampletime, &n, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    /* unpack the node stats */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(collect_resusage_sample, &nst, &n, OPAL_NODE_STAT))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    if (mca_sensor_resusage_component.log_node_stats) {
        if (NULL != nst) {
            res_log_node_stats(nst, node, sampletime);
        }
    }

    if (mca_sensor_resusage_component.log_process_stats) {
        /* unpack all process stats */
        n=1;
        while (OPAL_SUCCESS == (rc = opal_dss.unpack(collect_resusage_sample, &st, &n, OPAL_PSTAT))) {
            if (NULL != st) {
                res_log_process_stats(st, node, sampletime);
                OBJ_RELEASE(st);
            }
            n=1;
        }
        if (OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
            ORTE_ERROR_LOG(rc);
        }
    }

cleanup:
    SAFEFREE(node);
    if (NULL != nst) {
        OBJ_RELEASE(nst);
    }
    if (NULL != st) {
        OBJ_RELEASE(st);
    }
}

static void generate_test_vector(opal_buffer_t *v)
{
    int ret;
    const char *ctmp = "resusage";
    struct timeval current_time;
    opal_pstats_t *stats;
    opal_node_stats_t *nstats;

    /* pack the plugin name */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &ctmp, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        return;
    }

    /* pack the hostname */
    if (OPAL_SUCCESS != (ret =
                opal_dss.pack(v, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING))){
        ORTE_ERROR_LOG(ret);
        return;
    }

    /* get the sample time */
    gettimeofday(&current_time, NULL);

    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &current_time, 1, OPAL_TIMEVAL))) {
        ORTE_ERROR_LOG(ret);
        return;
    }

    /* update stats on ourself and the node */
    stats = OBJ_NEW(opal_pstats_t);
    nstats = OBJ_NEW(opal_node_stats_t);
    if (ORCM_SUCCESS != (ret = opal_pstat.query(orte_process_info.pid, stats, nstats))) {
        ORTE_ERROR_LOG(ret);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    /* will overwrite only node stats because actual process stats are needed for performance */
    if (NULL != nstats) {
    /* set the memory values to something identifiable for testing */
            nstats->total_mem = 100.1;
            nstats->free_mem = 200.2;
            nstats->buffers = 300.3;
            nstats->cached = 400.4;
            nstats->swap_cached = 500.5;
            nstats->swap_total = 600.6;
            nstats->swap_free = 700.7;
            nstats->mapped = 800.8;
            /* set the load averages */
            nstats->la = 1.0;
            nstats->la5 = 5.0;
            nstats->la15 = 15.0;
    }

    /* the stats framework can't know nodename or rank */
    strncpy(stats->node, orcm_sensor_base.host_tag_value, (OPAL_PSTAT_MAX_STRING_LEN - 1));
    stats->rank = ORTE_PROC_MY_NAME->vpid;

    /* pack the node stats */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &nstats, 1, OPAL_NODE_STAT))) {
        ORTE_ERROR_LOG(ret);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    /* pack the process stats */
    if (OPAL_SUCCESS != (ret = opal_dss.pack(v, &stats, 1, OPAL_PSTAT))) {
        ORTE_ERROR_LOG(ret);
        OBJ_RELEASE(stats);
        OBJ_RELEASE(nstats);
        return;
    }

    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
            "%s sensor:resusage: Test vector called",
            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(stats);
    OBJ_RELEASE(nstats);
}

static void res_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    static char* sensor_names[] = {
      /* Node Stats */
        "total_mem",
        "free_mem",
        "buffers",
        "cached",
        "swap_total",
        "swap_free",
        "mapped",
        "swap_cached",
        "la",
        "la5",
        "la15",
      /* Proc Stats */
        "rank",
        "pid",
        "cmd",
        "state",
        "percent_cpu",
        "priority",
        "num_threads",
        "vsize",
        "rss",
        "peak_vsize",
        "processor"
    };
    unsigned int tot_items = 23;
    unsigned int i = 0;
    const char *comp = "resusage";
    char *comp_data = NULL;
    int rc = OPAL_SUCCESS;

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    --tot_items; /* adjust out "hostname"/nodename pair */

    /* store our hostname */
    comp = "hostname";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot,
                         &orcm_sensor_base.host_tag_value, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    for(i = 0; i < tot_items; ++i) {
        asprintf(&comp_data, "sensor_resusage_%d", i+1);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp_data, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            free(comp_data);
            return;
        }
        free(comp_data);
        comp_data = sensor_names[i];
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp_data, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    OBJ_RELEASE(kvs);
}

static void res_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 0;
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;
    orcm_value_t *time_stamp;
    struct timeval current_time;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    gettimeofday(&current_time, NULL);
    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    if (NULL == time_stamp) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return;
    }
    records = OBJ_NEW(opal_list_t);
    opal_list_append(records, (opal_list_item_t*)time_stamp);
    while(tot_items > 0) {
        char *inv = NULL;
        char *inv_val = NULL;
        orcm_value_t *mkv = NULL;

        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(records);
            return;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            SAFEFREE(inv);
            OBJ_RELEASE(records);
            return;
        }

        mkv = OBJ_NEW(orcm_value_t);
        mkv->value.key = inv;
        mkv->value.type = OPAL_STRING;
        mkv->value.data.string = inv_val;
        opal_list_append(records, (opal_list_item_t*)mkv);

        --tot_items;
    }
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
    } else {
        my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    }
}

int resusage_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_resusage_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int resusage_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_resusage_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int resusage_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_resusage_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}

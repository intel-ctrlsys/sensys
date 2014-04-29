/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
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

#include "orte/mca/db/db.h"
#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/odls/odls_types.h"
#include "orte/mca/odls/base/odls_private.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/state/state.h"
#include "orte/runtime/orte_globals.h"
#include "orte/orted/orted.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_resusage.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void sample(void);
static void res_log(opal_buffer_t *sample);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_resusage_module = {
    init,
    finalize,
    NULL,
    NULL,
    sample,
    res_log
};

static bool log_enabled = true;
static orte_node_t *my_node;
static orte_proc_t *my_proc;

static int init(void)
{
    orte_job_t *jdata;

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
    return;
}

static void sample(void)
{
    opal_pstats_t *stats, *st;
    opal_node_stats_t *nstats, *nst;
    int rc, i;
    orte_proc_t *child, *hog=NULL;
    float in_use, max_mem;
    opal_buffer_t buf, *bptr;
    char *comp;

    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "sample:resusage sampling resource usage"));
    
    /* setup a buffer for our stats */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    /* pack our name */
    comp = strdup("resusage");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return;
    }
    free(comp);

    /* update stats on ourself and the node */
    stats = OBJ_NEW(opal_pstats_t);
    nstats = OBJ_NEW(opal_node_stats_t);
    if (ORCM_SUCCESS != (rc = opal_pstat.query(orte_process_info.pid, stats, nstats))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(stats);
        OBJ_RELEASE(nstats);
        OBJ_DESTRUCT(&buf);
        return;
    }

    /* the stats framework can't know nodename or rank */
    strncpy(stats->node, orte_process_info.nodename, OPAL_PSTAT_MAX_STRING_LEN);
    stats->rank = ORTE_PROC_MY_NAME->vpid;
    /* locally save the stats */
    if (NULL != (st = (opal_pstats_t*)opal_ring_buffer_push(&my_proc->stats, stats))) {
        OBJ_RELEASE(st);
    }
    if (NULL != (nst = (opal_node_stats_t*)opal_ring_buffer_push(&my_node->stats, nstats))) {
        /* release the popped value */
        OBJ_RELEASE(nst);
    }

    /* pack them */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &nstats, 1, OPAL_NODE_STAT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &stats, 1, OPAL_PSTAT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return;
    }

    /* loop through our children and update their stats */
    if (NULL != orte_local_children) {
        for (i=0; i < orte_local_children->size; i++) {
            if (NULL == (child = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i))) {
                continue;
            }
            if (!child->alive) {
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
                OBJ_RELEASE(stats);
                continue;
            }
            /* the stats framework can't know nodename or rank */
            strncpy(stats->node, orte_process_info.nodename, OPAL_PSTAT_MAX_STRING_LEN);
            stats->rank = child->name.vpid;
            /* store it */
            if (NULL != (st = (opal_pstats_t*)opal_ring_buffer_push(&child->stats, stats))) {
                OBJ_RELEASE(st);
            }
            /* pack them */
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&buf, &stats, 1, OPAL_PSTAT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&buf);
                return;
            }
        }
    }

    /* xfer any data for transmission */
    if (0 < buf.bytes_used) {
        bptr = &buf;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(orcm_sensor_base.samples, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&buf);
            return;
        }
    }
    OBJ_DESTRUCT(&buf);

    /* are there any issues with node-level usage? */
    nst = (opal_node_stats_t*)opal_ring_buffer_poke(&my_node->stats, -1);
    if (NULL != nst && 0.0 < mca_sensor_resusage_component.node_memory_limit) {
        OPAL_OUTPUT_VERBOSE((2, orcm_sensor_base_framework.framework_output,
                             "%s CHECKING NODE MEM",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        /* compute the percentage of node memory in-use */
        in_use = 1.0 - (nst->free_mem / nst->total_mem);
        OPAL_OUTPUT_VERBOSE((2, orcm_sensor_base_framework.framework_output,
                             "%s PERCENT USED: %f LIMIT: %f",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             in_use, mca_sensor_resusage_component.node_memory_limit));
        if (mca_sensor_resusage_component.node_memory_limit <= in_use) {
            /* loop through our children and find the biggest hog */
            hog = NULL;
            max_mem = 0.0;
            for (i=0; i < orte_local_children->size; i++) {
                if (NULL == (child = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i))) {
                    continue;
                }
                if (!child->alive) {
                    continue;
                }
                if (0 == child->pid) {
                    /* race condition */
                    continue;
                }
                if (NULL == (st = (opal_pstats_t*)opal_ring_buffer_poke(&child->stats, -1))) {
                    continue;
                }
                OPAL_OUTPUT_VERBOSE((5, orcm_sensor_base_framework.framework_output,
                                     "%s PROC %s AT VSIZE %f",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&child->name), st->vsize));
                if (max_mem < st->vsize) {
                    hog = child;
                    max_mem = st->vsize;
                }
            }
            if (NULL == hog) {
                /* if all children dead and we are still too big,
                 * then we must be the culprit - abort
                 */
                OPAL_OUTPUT_VERBOSE((2, orcm_sensor_base_framework.framework_output,
                                     "%s NO CHILD: COMMITTING SUICIDE",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
                orte_errmgr.abort(ORCM_ERR_MEM_LIMIT_EXCEEDED, NULL);
            } else {
                /* report the problem */
                OPAL_OUTPUT_VERBOSE((2, orcm_sensor_base_framework.framework_output,
                                     "%s REPORTING %s TO ERRMGR FOR EXCEEDING LIMITS",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&hog->name)));
                ORTE_ACTIVATE_PROC_STATE(&hog->name, ORTE_PROC_STATE_SENSOR_BOUND_EXCEEDED);
            }
            /* since we have ordered someone to die, we've done enough for this
             * time around - don't check proc limits as well
             */
            return;
        }
    }

    /* check proc limits */
    if (0.0 < mca_sensor_resusage_component.proc_memory_limit) {
        OPAL_OUTPUT_VERBOSE((2, orcm_sensor_base_framework.framework_output,
                             "%s CHECKING PROC MEM",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        /* check my children first */
        for (i=0; i < orte_local_children->size; i++) {
            if (NULL == (child = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i))) {
                continue;
            }
            if (!child->alive) {
                continue;
            }
            if (0 == child->pid) {
                /* race condition */
                continue;
            }
            if (NULL == (st = (opal_pstats_t*)opal_ring_buffer_poke(&child->stats, -1))) {
                continue;
            }
            OPAL_OUTPUT_VERBOSE((5, orcm_sensor_base_framework.framework_output,
                                 "%s PROC %s AT VSIZE %f",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_NAME_PRINT(&child->name), st->vsize));
            if (mca_sensor_resusage_component.proc_memory_limit <= st->vsize) {
                /* report the problem */
                ORTE_ACTIVATE_PROC_STATE(&child->name, ORTE_PROC_STATE_SENSOR_BOUND_EXCEEDED);
            }
        }
    }
}

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void res_log(opal_buffer_t *sample)
{
    opal_pstats_t *st=NULL;
    opal_node_stats_t *nst=NULL;
    int rc, n;
    opal_list_t *vals;
    opal_value_t *kv;
    char *node;

    if (!log_enabled) {
        return;
    }

    /* unpack the node name */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &node, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* unpack the node stats */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nst, &n, OPAL_NODE_STAT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if (mca_sensor_resusage_component.log_node_stats) {
        vals = OBJ_NEW(opal_list_t);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_TIMEVAL;
        kv->data.tv.tv_sec = nst->sample_time.tv_sec;
        kv->data.tv.tv_usec = nst->sample_time.tv_usec;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = "hostname";
        kv->type = OPAL_STRING;
        kv->data.string = strdup(node);
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("total_mem");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->total_mem;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("free_mem");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->free_mem;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("buffers");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->buffers;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cached");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->cached;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_total");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->swap_total;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_free");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->swap_free;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("mapped");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->mapped;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_cached");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->swap_cached;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("la");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->la;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("la5");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->la5;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("la15");
        kv->type = OPAL_FLOAT;
        kv->data.fval = nst->la15;
        opal_list_append(vals, &kv->super);

        /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orte_db.store(orcm_sensor_base.dbhandle, "nodestats", vals, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(vals);
        }
    }

    OBJ_RELEASE(nst);

    if (mca_sensor_resusage_component.log_process_stats) {
        /* unpack all process stats */
        n=1;
        while (OPAL_SUCCESS == (rc = opal_dss.unpack(sample, &st, &n, OPAL_PSTAT))) {
            vals = OBJ_NEW(opal_list_t);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("node");
            kv->type = OPAL_STRING;
            kv->data.string = strdup(st->node);
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("rank");
            kv->type = OPAL_INT32;
            kv->data.int32 = st->rank;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("pid");
            kv->type = OPAL_PID;
            kv->data.pid = st->pid;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("cmd");
            kv->type = OPAL_STRING;
            kv->data.string = strdup(st->cmd);
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("state");
            kv->type = OPAL_STRING;
            kv->data.string = (char*)malloc(3 * sizeof(char));
            kv->data.string[0] = st->state[0];
            kv->data.string[1] = st->state[1];
            kv->data.string[2] = '\0';
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("time");
            kv->type = OPAL_TIMEVAL;
            kv->data.tv.tv_sec = st->time.tv_sec;
            kv->data.tv.tv_usec = st->time.tv_usec;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("percent_cpu");
            kv->type = OPAL_FLOAT;
            kv->data.fval = st->percent_cpu;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("priority");
            kv->type = OPAL_INT32;
            kv->data.int32 = st->priority;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("num_threads");
            kv->type = OPAL_INT16;
            kv->data.int16 = st->num_threads;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("vsize");
            kv->type = OPAL_FLOAT;
            kv->data.fval = st->vsize;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("rss");
            kv->type = OPAL_FLOAT;
            kv->data.fval = st->rss;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("peak_vsize");
            kv->type = OPAL_FLOAT;
            kv->data.fval = st->peak_vsize;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("processor");
            kv->type = OPAL_INT16;
            kv->data.int16 = st->processor;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("sample_time");
            kv->type = OPAL_TIMEVAL;
            kv->data.tv.tv_sec = st->sample_time.tv_sec;
            kv->data.tv.tv_usec = st->sample_time.tv_usec;
            opal_list_append(vals, &kv->super);

            /* store it */
            if (0 <= orcm_sensor_base.dbhandle) {
                orte_db.store(orcm_sensor_base.dbhandle, "procstats", vals, mycleanup, NULL);
            } else {
                OPAL_LIST_RELEASE(vals);
            }
            OBJ_RELEASE(st);
            n=1;
        }
        if (OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
            ORTE_ERROR_LOG(rc);
        }
    }
}

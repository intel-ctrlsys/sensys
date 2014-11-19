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
#include "opal/util/os_path.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_dmidata.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void dmidata_sample(orcm_sensor_sampler_t *sampler);
static void dmidata_log(opal_buffer_t *buf);
static void dmidata_inventory_collect(orcm_sensor_inventory_record_t *record);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_dmidata_module = {
    init,
    finalize,
    start,
    stop,
    dmidata_sample,
    dmidata_log,
    dmidata_inventory_collect
};

typedef struct {
    opal_list_item_t super;
    char *file;
    int socket;
    char *label;
    float critical_temp;    /* Remove these unnecessary buckets*/
    float max_temp;
} coretemp_tracker_t;
static void ctr_con(coretemp_tracker_t *trk)
{
    trk->file = NULL;
    trk->label = NULL;
}
static void ctr_des(coretemp_tracker_t *trk)
{
    if (NULL != trk->file) {
        free(trk->file);
    }
    if (NULL != trk->label) {
        free(trk->label);
    }
}
OBJ_CLASS_INSTANCE(coretemp_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

static bool log_enabled = true;
static opal_list_t tracking;

/* FOR FUTURE: extend to read cooling device speeds in
 *     current speed: /sys/class/thermal/cooling_deviceN/cur_state
 *     max speed: /sys/class/thermal/cooling_deviceN/max_state
 *     type: /sys/class/thermal/cooling_deviceN/type
 */
static int init(void)
{
    DIR *cur_dirp = NULL;

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/sys/class/dmi/id"))) {
        OBJ_DESTRUCT(&tracking);
        orte_show_help("help-orcm-sensor-dmidata.txt", "req-dir-not-found",
                       true, orte_process_info.nodename,
                       "/sys/class/dmi/id");
        return ORTE_ERROR;
    }

    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */

    closedir(cur_dirp);

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_LIST_DESTRUCT(&tracking);
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    return;
}

static void ipmi_inventory_collect(orcm_sensor_inventory_record_t *record)
{
    opal_output(0,"Inside DMIDATA inventory collection");
}

static void stop(orte_jobid_t jobid)
{
    return;
}


static void dmidata_sample(orcm_sensor_sampler_t *sampler)
{
    int ret, depth;    
    char *temp;    
    opal_buffer_t data, *bptr;
    hwloc_obj_t obj;
    uint32_t k, nprocs;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    bool packed;
    struct tm *sample_time;


    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    temp = strdup("dmidata");
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

    opal_hwloc_base_get_topology();

    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(opal_hwloc_topology, HWLOC_OBJ_MACHINE, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-machine", true);
        ORTE_ERROR_LOG(ORTE_ERROR);
        OBJ_DESTRUCT(&data);
        return;
    }
    if (NULL != obj) {
        /* Pack the total MACHINE Stats present and to be copied */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos_count, 1, OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        for (k=0; k < obj->infos_count; k++) {
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                                "%s : %s",obj->infos[k].name, obj->infos[k].value);
            /* Metric Name */
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos[k].name, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);                
                return;
            }

            /* Metric Value*/
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos[k].value, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);                
                return;
            }
        }
    } else {
        k = 0;
        /* Packed '0' MACHINE Stats */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &k, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    depth = hwloc_get_type_depth(opal_hwloc_topology, HWLOC_OBJ_SOCKET);
    if(depth >= 0) {
        nprocs = hwloc_get_nbobjs_by_depth(opal_hwloc_topology, depth);        
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &nprocs, 1, OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);            
            return;
        }
    }


    /* Pack SOCKET Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(opal_hwloc_topology, HWLOC_OBJ_SOCKET, 0))) {
        /* there are no sockets identified in this machine */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-sockets", true);
        ORTE_ERROR_LOG(ORTE_ERROR);
        OBJ_DESTRUCT(&data);
        return;
    }
    if (NULL != obj) {
        /* Pack the total SOCKET Stats present and to be copied */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos_count, 1, OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        for (k=0; k < obj->infos_count; k++) {            
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                                "%s : %s",obj->infos[k].name, obj->infos[k].value);
            /* Metric Name */
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos[k].name, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                free(timestamp_str);
                return;
            }

            /* Metric Value*/            
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &obj->infos[k].value, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                free(timestamp_str);
                return;
            }
        }
    } else {
        k = 0;
        /* Packed '0' SOCKET Stats */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &k, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
    packed = true;
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

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void dmidata_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    char *samplename, *samplevalue;
    int rc;
    int32_t n;
    uint32_t machine_info_count, socket_info_count, nprocs;
    opal_list_t *vals;
    opal_value_t *kv;
    uint32_t i;

    if (!log_enabled) {
        return;
    }

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

    /* xfr to storage */
    vals = OBJ_NEW(opal_list_t);

    /* load the sample time at the start */
    if (NULL == sampletime) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(sampletime);
    free(sampletime);
    opal_list_append(vals, &kv->super);

    /* load the hostname */
    if (NULL == hostname) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hostname");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(hostname);
    opal_list_append(vals, &kv->super);

    /* Add MACHINE level DMI Stats - 1*/
    /* Unpack infos_count */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &machine_info_count, &n, OPAL_UINT32))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    for (i=0; i < machine_info_count; i++) {
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &samplename, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &samplevalue, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        if (strstr(samplevalue,"....")) {
            strcpy(samplevalue,"N/A");
            opal_output(0,"%s: %s",samplename, samplevalue);
        } else {
            opal_output(0,"%s: %s",samplename, samplevalue);
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup(samplename);
        kv->type = OPAL_STRING;
        kv->data.string = strdup(samplevalue);
        opal_list_append(vals, &kv->super);

        free(samplename);
        free(samplevalue);
    }
    /* Add SOCKET level DMI Stats - 2*/
    /*Unpack Total num of Sockets/Processors */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &nprocs, &n, OPAL_UINT32))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    opal_output(0, "Total Sockets/Processors Present: %d", nprocs);
    /* Unpack infos_count */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &socket_info_count, &n, OPAL_UINT32))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    for (i=0; i < socket_info_count; i++) {
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &samplename, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &samplevalue, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        if (strstr(samplevalue,"....")) {
            strcpy(samplevalue,"N/A");
            opal_output(0,"%s: %s",samplename, samplevalue);
        } else {
            opal_output(0,"%s: %s",samplename, samplevalue);
        }
        kv = OBJ_NEW(opal_value_t);        
        kv->key = strdup(samplename);
        kv->type = OPAL_STRING;
        
        kv->data.string = strdup(samplevalue);
        opal_list_append(vals, &kv->super);
        free(samplename);
        free(samplevalue);
    }

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store(orcm_sensor_base.dbhandle, "dmidata", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }

}

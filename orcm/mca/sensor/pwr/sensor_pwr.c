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
#include <ctype.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <math.h>

#include "opal_stdint.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/os_path.h"
#include "opal/util/output.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_pwr.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void pwr_sample(orcm_sensor_sampler_t *sampler);
static void pwr_log(opal_buffer_t *buf);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_pwr_module = {
    init,
    finalize,
    start,
    stop,
    pwr_sample,
    pwr_log
};

#define MSR_RAPL_POWER_UNIT		0x606

/*
 * Platform specific RAPL bitmasks.
 */
#define MSR_PKG_POWER_INFO	0x614
#define POWER_UNIT_OFFSET	0
#define POWER_UNIT_MASK		0x0F


typedef struct {
    opal_list_item_t super;
    char *file;
    int core;
    double units;
} corepwr_tracker_t;
static void ctr_con(corepwr_tracker_t *trk)
{
    trk->file = NULL;
}
static void ctr_des(corepwr_tracker_t *trk)
{
    if (NULL != trk->file) {
        free(trk->file);
    }
}
OBJ_CLASS_INSTANCE(corepwr_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

static bool log_enabled = true;
static opal_list_t tracking;

static int read_msr(int fd, long long *value, int offset)
{
    uint64_t data;

    if (pread(fd, &data, sizeof data, offset) != sizeof(data)) {
        return ORTE_ERROR;
    }
    *value = (long long)data;
    return ORCM_SUCCESS;
}
static int check_cpu_type(void);


static int init(void)
{
    int fd;
    DIR *cur_dirp = NULL;
    struct dirent *entry;
    corepwr_tracker_t *trk;
    long long units;

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /* we only handle certain cpu types as we have to know the binary
     * layout of the msr file
     */
    if (ORCM_SUCCESS != check_cpu_type() &&
        !mca_sensor_pwr_component.test) {
        orte_show_help("help-orcm-sensor-pwr.txt", "no-topo-info",
                       true, mca_sensor_pwr_component.model);
        return ORCM_ERR_NOT_SUPPORTED;
    }

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/dev/cpu"))) {
        OBJ_DESTRUCT(&tracking);
        orte_show_help("help-orcm-sensor-pwr.txt", "no-access",
                       true, orte_process_info.nodename,
                       "/dev/cpu");
        return ORTE_ERROR;
    }

    /*
     * For each directory
     */
    while (NULL != (entry = readdir(cur_dirp))) {
        
        /*
         * Skip the obvious
         */
        if (NULL == entry->d_name) {
            continue;
        }

        if (0 == strncmp(entry->d_name, ".", strlen(".")) ||
            0 == strncmp(entry->d_name, "..", strlen(".."))) {
            continue;
        }

        /* if it contains anything other than a digit, then it isn't a cpu directory */
        if (!isdigit(entry->d_name[strlen(entry->d_name)-1])) {
            continue;
        }

        /* track the info for this core */
        trk = OBJ_NEW(corepwr_tracker_t);
        trk->core = strtoul(entry->d_name, NULL, 10);
        trk->file = opal_os_path(false, "/dev/cpu", entry->d_name, "msr", NULL);

        if (!mca_sensor_pwr_component.test) {
            /* get the power units for this core */
            if (0 >= (fd = open(trk->file, O_RDONLY))) {
                /* can't access file */
                OBJ_RELEASE(trk);
                continue;
            }
            if (ORCM_SUCCESS != read_msr(fd, &units, MSR_RAPL_POWER_UNIT)) {
                /* can't read required info */
                OBJ_RELEASE(trk);
                close(fd);
                continue;
            }
            trk->units = pow(0.5,(double)(units & POWER_UNIT_MASK));
            close(fd);
        }

        /* add to our list */
        opal_list_append(&tracking, &trk->super);
    }
    closedir(cur_dirp);

    if (0 == opal_list_get_size(&tracking)) {
        /* nothing to read */
        orte_show_help("help-orcm-sensor-pwr.txt", "no-cores-found",
                       true, orte_process_info.nodename);
        return ORTE_ERROR;
    }

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


static void stop(orte_jobid_t jobid)
{
    return;
}

static void pwr_sample(orcm_sensor_sampler_t *sampler)
{
    corepwr_tracker_t *trk, *nxt;
    opal_buffer_t data, *bptr;
    int32_t ncores;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    long long value;
    int fd, ret;
    float power;
    char *temp;
    bool packed;
    struct tm *sample_time;

    if (0 == opal_list_get_size(&tracking) && !mca_sensor_pwr_component.test) {
        return;
    }

    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "%s sampling power",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* prep to store the results */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    packed = false;

    /* pack our name */
    temp = strdup("pwr");
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

    /* store the number of cores */
    ncores = (int32_t)opal_list_get_size(&tracking);
    if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &ncores, 1, OPAL_INT32))) {
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

    OPAL_LIST_FOREACH_SAFE(trk, nxt, &tracking, corepwr_tracker_t) {
        if (mca_sensor_pwr_component.test) {
            power = 1.2345;
        } else {
            if (0 >= (fd = open(trk->file, O_RDONLY))) {
                /* disable this one - cannot read the file */
                opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                    "%s access denied to pwr file %s - removing it",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    trk->file);
                opal_list_remove_item(&tracking, &trk->super);
                OBJ_RELEASE(trk);
                continue;
            }
            if (ORCM_SUCCESS != read_msr(fd, &value, MSR_PKG_POWER_INFO)) {
                /* disable this one - cannot read the file */
                opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                    "%s failed to read pwr file %s - removing it",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    trk->file);
                opal_list_remove_item(&tracking, &trk->super);
                OBJ_RELEASE(trk);
                close(fd);
                continue;
            }
            power = trk->units * (double)(value & 0x7fff);
            close(fd);
        }
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &power, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
        packed = true;
    }

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

static void pwr_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    int rc;
    int32_t n, ncores;
    opal_list_t *vals;
    opal_value_t *kv;
    float fval;
    int i;

    if (!log_enabled) {
        return;
    }

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    /* and the number of cores on that host */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &ncores, &n, OPAL_INT32))) {
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
                        "%s Received power log from host %s with %d cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname, ncores);

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

    for (i=0; i < ncores; i++) {
        kv = OBJ_NEW(opal_value_t);
        asprintf(&kv->key, "core%d", i);
        kv->type = OPAL_FLOAT;
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);
    }

    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        /* the database framework will release the values */
        orcm_db.store(orcm_sensor_base.dbhandle, "pwr", vals, mycleanup, NULL);
    } else {
        /* cleanup the xfr storage */
        OPAL_LIST_RELEASE(vals);
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }

}


/* list of supporcmd chipsets */
#define CPU_SANDYBRIDGE		42
#define CPU_SANDYBRIDGE_EP	45
#define CPU_IVYBRIDGE		58
#define CPU_IVYBRIDGE_EP	62
#define CPU_HASWELL		60


/* go thru our topology and check the sockets
 * to see if they contain a match - at this time,
 * we don't support hetero sockets, so any mismatch
 * will disqualify us
 */ 
static int check_cpu_type(void)
{
    hwloc_obj_t obj;
    unsigned k;

    if (NULL == (obj = hwloc_get_obj_by_type(opal_hwloc_topology, HWLOC_OBJ_SOCKET, 0))) {
        /* there are no sockets identified in this machine */
        orte_show_help("help-orcm-sensor-pwr.txt", "no-sockets", true);
        return ORTE_ERROR;
    }

    while (NULL != obj) {
        for (k=0; k < obj->infos_count; k++) {
            if (0 == strcmp(obj->infos[k].name, "model") &&
                NULL != obj->infos[k].value) {
                mca_sensor_pwr_component.model = strtoul(obj->infos[k].value, NULL, 10);
                
                switch (mca_sensor_pwr_component.model) {
		case CPU_SANDYBRIDGE:
                    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                        "sensor:pwr Found Sandybridge CPU");
                    return ORCM_SUCCESS;
		case CPU_SANDYBRIDGE_EP:
                    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                        "sensor:pwr Found Sandybridge-EP CPU");
                    return ORCM_SUCCESS;
		case CPU_IVYBRIDGE:
                    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                        "sensor:pwr Found Ivybridge CPU");
                    return ORCM_SUCCESS;
		case CPU_IVYBRIDGE_EP:
                    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                        "sensor:pwr Found Ivybridge-EP CPU");
                    return ORCM_SUCCESS;
		case CPU_HASWELL:
                    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                        "sensor:pwr Found Haswell CPU");
                    return ORCM_SUCCESS;
		default:
                    orte_show_help("help-orcm-sensor-pwr.txt", "unsupporcmd-model",
                                   true, mca_sensor_pwr_component.model);
                    return ORTE_ERROR;
                }
            }
        }
        obj = obj->next_sibling;
    }
    return ORTE_ERROR;
}

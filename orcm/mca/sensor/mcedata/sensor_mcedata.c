/*
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
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
#include "orte/mca/notifier/notifier.h"
#include "orte/mca/notifier/base/base.h"

#include "orcm/mca/db/db.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_mcedata.h"

#define     MCE_REG_COUNT   5

#define     MCG_STATUS  0
#define     MCG_CAP     1
#define     MCI_STATUS  2
#define     MCI_ADDR    3
#define     MCI_MISC    4

#define GEN_CACHE_ERR_MASK  (1<<3)
#define TLB_ERR_MASK        (1<<4)
#define MEM_CTRL_ERR_MASK   (1<<7)
#define CACHE_ERR_MASK      (1<<8)
#define BUS_IC_ERR_MASK     (1<<11)

typedef enum _mcetype {
    e_gen_cache_error,
    e_tlb_error,
    e_mem_ctrl_error,
    e_cache_error,
    e_bus_ic_error,
    e_unknown_error
} mcetype;

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void mcedata_sample(orcm_sensor_sampler_t *sampler);
static void mcedata_log(opal_buffer_t *buf);

static void mcedata_decode(unsigned long *mce_reg, opal_list_t *vals);
mcetype get_mcetype(uint64_t mci_status);
static void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
static void mcedata_tlb_filter(unsigned long *mce_reg, opal_list_t *vals);
static void mcedata_mem_ctrl_filter(unsigned long *mce_reg, opal_list_t *vals);
static void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
static void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_mcedata_module = {
    init,
    finalize,
    start,
    stop,
    mcedata_sample,
    mcedata_log,
    NULL,
    NULL
};

typedef struct {
    opal_list_item_t super;
    char *file;
    int socket;
    int core;
    char *label;
    float critical_temp;
    float max_temp;
} mcedata_tracker_t;

static void ctr_con(mcedata_tracker_t *trk)
{
    trk->file = NULL;
    trk->label = NULL;
    trk->socket = -1;
    trk->core = -1;
}
static void ctr_des(mcedata_tracker_t *trk)
{
    if (NULL != trk->file) {
        free(trk->file);
    }
    if (NULL != trk->label) {
        free(trk->label);
    }
}
OBJ_CLASS_INSTANCE(mcedata_tracker_t,
                   opal_list_item_t,
                   ctr_con, ctr_des);

static bool log_enabled = true;
static opal_list_t tracking;
static bool mcelog_avail = false;
char mce_reg_name [MCE_REG_COUNT][12]  = {
    "MCG_STATUS",
    "MCG_CAP",
    "MCI_STATUS",
    "MCI_ADDR",
    "MCI_MISC"
};

static char *orte_getline(FILE *fp)
{
    char *ret, *buff;
    char input[1024];

    ret = fgets(input, 1024, fp);
    if (NULL != ret) {
	   input[strlen(input)-1] = '\0';  /* remove newline */
	   buff = strdup(input);
	   return buff;
    }
    
    return NULL;
}


/* mcedata is a special sensor that has to be called on it's own separate thread
 * It is necessary to catch the machine check errors in real time and send it up
 * to the aggregator for processing
 */

/* FOR FUTURE: extend to read cooling device speeds in
 *     current speed: /sys/class/thermal/cooling_deviceN/cur_state
 *     max speed: /sys/class/thermal/cooling_deviceN/max_state
 *     type: /sys/class/thermal/cooling_deviceN/type
 */
static int init(void)
{
    DIR *cur_dirp = NULL, *tdir;
    struct dirent *dir_entry, *entry;
    char *dirname = NULL;
    char *filename, *ptr, *tmp;
    size_t tlen = strlen("temp");
    size_t ilen = strlen("_input");
    FILE *fp;
    mcedata_tracker_t *trk, *t2;
    bool inserted;
    char *skt;

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/dev"))) {
        OBJ_DESTRUCT(&tracking);
        orte_show_help("help-orcm-sensor-mcedata.txt", "req-dir-not-found",
                       true, orte_process_info.nodename,
                       "/dev");
        return ORTE_ERROR;
    }

    /*
     * For each directory
     */
    while (NULL != (dir_entry = readdir(cur_dirp))) {

        skt = strchr(dir_entry->d_name, '.');
        if (NULL != skt) {
            skt++;
        }

        /* open that directory */
        if (NULL == (dirname = opal_os_path(false, "/dev", dir_entry->d_name, NULL ))) {
            continue;
        }
        if (0 == strcmp(dirname, "/dev/mcelog"))
        {
            opal_output(0,"Dirname: %s", dirname);
            mcelog_avail = true;
        }
        free(dirname);
    }
    closedir(cur_dirp);

    if (mcelog_avail != true ) {
        /* nothing to read */
        orte_show_help("help-orcm-sensor-mcedata.txt", "no-mcelog",
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

mcetype get_mcetype(uint64_t mci_status)
{
    if ( mci_status & BUS_IC_ERR_MASK) {
        return e_bus_ic_error;
    } else if (mci_status & CACHE_ERR_MASK) {
        return e_cache_error;
    } else if (mci_status & MEM_CTRL_ERR_MASK) {
        return e_mem_ctrl_error;
    } else if (mci_status & TLB_ERR_MASK) {
        return e_tlb_error;
    } else if (mci_status & GEN_CACHE_ERR_MASK) {
        return e_gen_cache_error;
    } else {
        return e_unknown_error;
    }

}

static void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 0 - Generic Cache Hierarchy Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("GenCacheError");
    opal_list_append(vals, &kv->super);

    /* Get the cache level */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hierarchy_level");
    kv->type = OPAL_STRING;
    switch (mce_reg[MCI_STATUS] & 0x3) {
        case 0: kv->data.string = strdup("Level 0"); break;
        case 1: kv->data.string = strdup("Level 1"); break;
        case 2: kv->data.string = strdup("Level 2"); break;
        case 3: kv->data.string = strdup("Generic"); break;
    }
    opal_list_append(vals, &kv->super);
}

static void mcedata_tlb_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 1 - TLB Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("TLBError");
    opal_list_append(vals, &kv->super);

}

static void mcedata_mem_ctrl_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 2 - Memory Controller Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("MemCtrlError");
    opal_list_append(vals, &kv->super);

}

static void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 3 - Cache Hierarchy Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("CacheError");
    opal_list_append(vals, &kv->super);

    /* Get the cache level */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hierarchy_level");
    kv->type = OPAL_STRING;
    switch (mce_reg[MCI_STATUS] & 0x3) {
        case 0: kv->data.string = strdup("L0"); break;
        case 1: kv->data.string = strdup("L1"); break;
        case 2: kv->data.string = strdup("L2"); break;
        case 3: kv->data.string = strdup("G"); break;
    }
    opal_list_append(vals, &kv->super);

    /* Transaction Type */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("transaction_type");
    kv->type = OPAL_STRING;
    switch ((mce_reg[MCI_STATUS] & 0xC) >> 2) {
        case 0: kv->data.string = strdup("I"); break; /* Instruction */
        case 1: kv->data.string = strdup("D"); break; /* Data */
        case 2: kv->data.string = strdup("G"); break; /* Generic */
        default: kv->data.string = strdup("Unknown"); break;
    }
    opal_list_append(vals, &kv->super);

    /* Request Type */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("request_type");
    kv->type = OPAL_STRING;
    switch ((mce_reg[MCI_STATUS] & 0xF0) >> 4) {
        case 0: kv->data.string = strdup("ERR"); break;     /* Generic Error */
        case 1: kv->data.string = strdup("RD"); break;      /* Generic Read */
        case 2: kv->data.string = strdup("WR"); break;      /* Generic Write */
        case 3: kv->data.string = strdup("DRD"); break;     /* Data Read */
        case 4: kv->data.string = strdup("DWR"); break;     /* Data Write */
        case 5: kv->data.string = strdup("IRD"); break;     /* Instruction Read */
        case 6: kv->data.string = strdup("PREFETCH"); break;/* Prefetch */
        case 7: kv->data.string = strdup("EVICT"); break;   /* Evict */
        case 8: kv->data.string = strdup("SNOOP"); break;   /* Snoop */
        default: kv->data.string = strdup("Unknown"); break;
        
    }
    opal_list_append(vals, &kv->super);




}

static void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 4 - Bus and Interconnect Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("BusICError");
    opal_list_append(vals, &kv->super);

}

static void mcedata_unknown_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output(0,"MCE Error Type 5 - Unknown Error");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ErrorLocation");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("Unknown Error");
    opal_list_append(vals, &kv->super);

}

static void mcedata_decode(unsigned long *mce_reg, opal_list_t *vals)
{
    int i = 0;
    mcetype type;
    while (i < 5)
    {
        opal_output_verbose(5,  orcm_sensor_base_framework.framework_output,
                                "Value: %lx - %s", mce_reg[i], mce_reg_name[i]);
        i++;
    }

    type = get_mcetype(mce_reg[MCI_STATUS]);
    switch (type) {
        case 0 :    mcedata_gen_cache_filter(mce_reg, vals);
                    break;
        case 1 :    mcedata_tlb_filter(mce_reg, vals);
                    break;
        case 2 :    mcedata_mem_ctrl_filter(mce_reg, vals);
                    break;
        case 3 :    mcedata_cache_filter(mce_reg, vals);
                    break;
        case 4 :    mcedata_bus_ic_filter(mce_reg, vals);
                    break;
        case 5 :    mcedata_unknown_filter(mce_reg, vals);
                    break;
    }

}


static void mcedata_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *temp;
    opal_buffer_t data, *bptr;
    time_t now;
    char time_str[40];
    char *timestamp_str;
    bool packed;
    struct tm *sample_time;
    uint32_t i = 0;
    uint64_t mce_reg[MCE_REG_COUNT];
    int count = 0;

    while (0 == count) {
        i = 0;
        count++;
        /* prep to store the results */
        OBJ_CONSTRUCT(&data, opal_buffer_t);
        packed = false;

        /* pack our name */
        temp = strdup("mcedata");
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

        /* Block here for a read call to the mcedata log file */
        /* read = ();
         */
        mce_reg[MCG_STATUS] = 0x0;
        mce_reg[MCG_CAP]    = 0x1;
        mce_reg[MCI_STATUS] = 0x8820000000000105;
        mce_reg[MCI_ADDR]   = 0x3;
        mce_reg[MCI_MISC]   = 0x4;
        packed = true;

        while (i < 5) {
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "Packing %s : %lu",mce_reg_name[i], mce_reg[i]);
            /* store the mce register */
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &mce_reg[i], 1, OPAL_UINT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                return;
            }
            i++;
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
}

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void mcedata_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    char *sampletime;
    int rc;
    int32_t n, i = 0;
    opal_list_t *vals;
    opal_value_t *kv;
    uint64_t mce_reg[MCE_REG_COUNT];

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
                        "%s Received log from host %s with xx cores",
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
        goto cleanup;
        return;
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("hostname");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(hostname);
    opal_list_append(vals, &kv->super);

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("teststr");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("XYZ");
    opal_list_append(vals, &kv->super);

    while (i < 5) {
        /* sample time */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &mce_reg[i], &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                    "Collected %s: %lu", mce_reg_name[i],mce_reg[i]);
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup(mce_reg_name[i]);
        kv->type = OPAL_UINT64;
        kv->data.uint64 = mce_reg[i];
        opal_list_append(vals, &kv->super);
        i++;
    }

    mcedata_decode(mce_reg, vals);


    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store(orcm_sensor_base.dbhandle, "mcedata", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }

}

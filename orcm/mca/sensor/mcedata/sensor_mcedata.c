/*
 * Copyright (c) 2015 -2016 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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
#include "opal/runtime/opal_progress_threads.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/notifier/notifier.h"
#include "orte/mca/notifier/base/base.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/analytics/analytics.h"
#include "orcm/util/utils.h"

#include "sensor_mcedata.h"

#define ON_FAILURE_GOTO(x,label) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);goto label;}
#define ON_FAILURE_RETURN(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);return;}
#define ON_NULL_RETURN(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return;}
#define ON_NULL_GOTO(x,label) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);goto label;}

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void mcedata_sample(orcm_sensor_sampler_t *sampler);
static void mcedata_log(opal_buffer_t *buf);

void mcedata_unknown_filter(unsigned long *mce_reg, opal_list_t *vals);
void mcedata_decode(unsigned long *mce_reg, opal_list_t *vals);
mcetype get_mcetype(uint64_t mci_status);
void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
void mcedata_tlb_filter(unsigned long *mce_reg, opal_list_t *vals);
static void mcedata_mem_ctrl_filter(unsigned long *mce_reg, opal_list_t *vals);
void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals);
void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals);
static void get_log_lines(FILE *fp);
static void perthread_mcedata_sample(int fd, short args, void *cbdata);
void collect_mcedata_sample(orcm_sensor_sampler_t *sampler);
static void mcedata_set_sample_rate(int sample_rate);
void mcedata_get_sample_rate(int *sample_rate);
static void mcedata_inventory_collect(opal_buffer_t *inventory_snapshot);
static void mcedata_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
int mcedata_enable_sampling(const char* sensor_specification);
int mcedata_disable_sampling(const char* sensor_specification);
int mcedata_reset_sampling(const char* sensor_specification);
static void generate_test_vector(orcm_sensor_sampler_t* sampler);
long orcm_sensor_mcedata_get_log_file_pos(void);

static char* decode_severity(uint64_t status);
static char* decode_hierarchy_level(uint64_t status);
static char* decode_address_mode(uint64_t status);
static char* decode_memory_io(uint64_t status);
static char* decode_timeout(uint64_t status);
static char* decode_request_type(uint64_t status);
static char* decode_participation(uint64_t status);
static char* decode_transaction_type(uint64_t status);
static char* decode_cache_health(uint64_t status);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_mcedata_module = {
    init,
    finalize,
    start,
    stop,
    mcedata_sample,
    mcedata_log,
    mcedata_inventory_collect,
    mcedata_inventory_log,
    mcedata_set_sample_rate,
    mcedata_get_sample_rate,
    mcedata_enable_sampling,
    mcedata_disable_sampling,
    mcedata_reset_sampling
};

static bool mcelog_avail = false;
static bool mce_default = false;
const char *mce_reg_name []  = {
    "MCG_STATUS",
    "MCG_CAP",
    "MCI_STATUS",
    "MCI_ADDR",
    "MCI_MISC"
};
static orcm_sensor_sampler_t *mcedata_sampler = NULL;

enum mcelogTag {mcelog_cpu, mcelog_bank, mcelog_misc, mcelog_addr,
      mcelog_status, mcelog_mcgstatus, mcelog_time,
      mcelog_socketid, mcelog_mcgcap, mcelog_sentinel};

static char *log_line_token[] = {
" CPU ",
" BANK ",
" MISC ",
" ADDR ",
" STATUS ",
" MCGSTATUS ",
" TIME ",
" SOCKETID ",
" MCGCAP "
};

/* Next position at which to read log file */
static long log_file_pos;

/* MCE log lines for reporting*/
static char* log_lines[mcelog_sentinel];

/* UT accessors only */
long orcm_sensor_mcedata_get_log_file_pos(void)
{
    return log_file_pos;
}

static void start_log_file(void)
{
    FILE *fp;
    long tot_bytes = -1;
    long ret;
    char buf[128];

    log_file_pos = 0;

    if (true != mca_sensor_mcedata_component.historical_collection) {
        /* If the user requires MCE collection from the point when sensor was
         * started, then we should ignore all the events logged prior to start
         * of sensor. */

        fp = fopen(mca_sensor_mcedata_component.logfile, "r");
        if (NULL != fp){
          ret = fseek(fp, 0, SEEK_END);

          if (0 == ret){
            tot_bytes = ftell(fp);
          }

          fclose(fp);
        }

        if (tot_bytes < 0){
            snprintf(buf, 127, "mcedata log file error: %s",strerror(errno));
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output, buf);
            return;
        }

        log_file_pos = tot_bytes;
    }
}

static long update_log_file_size(FILE *fp)
{
    long tot_bytes = -1;
    int ret;
    static int errorReported = 0;
    char buf[128];

    /* The caller is checking for fp = NULL */
    ret = fseek(fp, 0, SEEK_END);

    if (ret == 0) {
      tot_bytes = ftell(fp);
    }

    if (tot_bytes < 0) {
        if (!errorReported){
            snprintf(buf, 127, "mcedata log file error: %s",strerror(errno));
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                buf);
            errorReported = 1;
        }
        return -1;
    }

    /* In case log file rotates or is stashed and cleared, the index
     * at which mcedata looks for MCE events has to be reset too */

    if (tot_bytes < log_file_pos) {
        log_file_pos = 0;
    }

    return tot_bytes;
}

static void free_log_lines(void)
{
    for (int i = 0; i < mcelog_sentinel; i++) {
        SAFEFREE(log_lines[i]);
    }
}

static void get_log_lines(FILE *fp)
{
    int nextTag = 0;
    char *ptr=NULL;
    size_t n=0;

    while (nextTag < mcelog_sentinel && -1 != getline(&ptr, &n, fp)){

        if (strstr(ptr, "mcelog") && strstr(ptr, log_line_token[nextTag])){
            SAFEFREE(log_lines[nextTag]);
            log_lines[nextTag++] = ptr;
        }
        else{
            free(ptr);
        }

        ptr = NULL;
        n = 0;
    }

    if (nextTag > 0 && nextTag < mcelog_sentinel){

        // MCE log messages are several lines long.  We started to get an
        // mcelog message, but hit the end of file before getting it all.
        // In the rare case that this happens, we will miss that MC error.
        // Since MC errors tend to happen in multiples, we are not going to
        // handle this case unless it's requested.


        free_log_lines();
    }
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
    DIR *cur_dirp = NULL;
    struct dirent *dir_entry;
    char *dirname = NULL;
    char *skt;

    memset((void*)log_lines, 0, sizeof(log_lines));

    mca_sensor_mcedata_component.diagnostics = 0;
    mca_sensor_mcedata_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("mcedata", orcm_sensor_base.collect_metrics,
                                                mca_sensor_mcedata_component.collect_metrics);

    /* If the test vectors have been enabled then bypass the init */
    if (mca_sensor_mcedata_component.test) {
        return ORCM_SUCCESS;
    }

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/dev"))) {
        orte_show_help("help-orcm-sensor-mcedata.txt", "req-dir-not-found",
                       true, orte_process_info.nodename, "/dev");
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
        if (0 == strcmp(dir_entry->d_name, "mcelog"))
        {
            opal_output(0,"/dev/mcelog available");
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
    if(NULL==mca_sensor_mcedata_component.logfile) {
        mca_sensor_mcedata_component.logfile = strdup("/var/log/messages");
        mce_default = true;
    }
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    if(true == mce_default) {
        free(mca_sensor_mcedata_component.logfile);
        mca_sensor_mcedata_component.logfile = NULL;
    }

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_mcedata_component.runtime_metrics);
    mca_sensor_mcedata_component.runtime_metrics = NULL;

    free_log_lines();
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    start_log_file();

    /* start a separate mcedata progress thread for sampling */
    if (mca_sensor_mcedata_component.use_progress_thread) {
        if (!mca_sensor_mcedata_component.ev_active) {
            mca_sensor_mcedata_component.ev_active = true;
            if (NULL == (mca_sensor_mcedata_component.ev_base = opal_progress_thread_init("mcedata"))) {
                mca_sensor_mcedata_component.ev_active = false;
                return;
            }
        }

        /* setup mcedata sampler */
        mcedata_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if mcedata sample rate is provided for this*/
        if (!mca_sensor_mcedata_component.sample_rate) {
            mca_sensor_mcedata_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        mcedata_sampler->rate.tv_sec = mca_sensor_mcedata_component.sample_rate;
        mcedata_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(mca_sensor_mcedata_component.ev_base, &mcedata_sampler->ev,
                               perthread_mcedata_sample, mcedata_sampler);
        opal_event_evtimer_add(&mcedata_sampler->ev, &mcedata_sampler->rate);
    }else{
	 mca_sensor_mcedata_component.sample_rate = orcm_sensor_base.sample_rate;

    }

    return;
}


static void stop(orte_jobid_t jobid)
{
    if (mca_sensor_mcedata_component.ev_active) {
        mca_sensor_mcedata_component.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("mcedata");
        OBJ_RELEASE(mcedata_sampler);
    }
    return;
}

static void perthread_mcedata_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor mcedata : perthread_mcedata_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_mcedata_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if mcedata sample rate is provided for this*/
    if (mca_sensor_mcedata_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_mcedata_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
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

void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    const char* levels[4] = {
        "Level 0",
        "Level 1",
        "Level 2",
        "Generic"
    };
    orcm_value_t *sensor_metric;
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 0 - Generic Cache Hierarchy Errors");

    sensor_metric = orcm_util_load_orcm_value("ErrorLocation", "GenCacheError", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Get the cache level */
    sensor_metric = orcm_util_load_orcm_value("hierarchy_level", (char*)levels[mce_reg[MCI_STATUS] & 0x3], OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);
}

void mcedata_tlb_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    orcm_value_t *sensor_metric;
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 1 - TLB Errors");

    sensor_metric = orcm_util_load_orcm_value("error_location", "tlb_Error", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

}

static char* decode_severity(uint64_t status)
{
    static const char* severities[] = {
        "CE",      /* 0 */
        "UCNA",    /* 1 */
        "CE",      /* 2 */
        "UCNA",    /* 3 */
        "UNKNOWN", /* 4 */
        "UC",      /* 5 */
        "UNKNOWN", /* 6 */
        "UC",      /* 7 */
        "CE",      /* 8 */
        "SRAO",    /* 9 */
        "CE",      /* 10 */
        "SRAO",    /* 11 */
        "UNKNOWN", /* 12 */
        "UC",      /* 13 */
        "UNKNOWN", /* 14 */
        "UC",      /* 15 */
        "CE",      /* 16 */
        "UNKNOWN", /* 17 */
        "CE",      /* 18 */
        "UNKNOWN", /* 19 */
        "UNKNOWN", /* 20 */
        "UC",      /* 21 */
        "UNKNOWN", /* 22 */
        "UC",      /* 23 */
        "CE",      /* 24 */
        "SRAR",    /* 25 */
        "CE",      /* 26 */
        "CE",      /* 27 */
        "UNKNOWN", /* 28 */
        "UC",      /* 29 */
        "UNKNOWN", /* 30 */
        "UC"       /* 31 */
    };
    uint64_t index;
    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    /* Decode to first 5 bits in a number to use as an index lookup. */
    index = ((status & MCI_UC_MASK) >> 61) |    /* bit 0 */
            ((status & MCI_MISCV_MASK) >> 58) | /* bit 1 */
            ((status & MCI_PCC_MASK) >> 55) |   /* bit 2 */
            ((status & MCI_S_MASK) >> 53) |     /* bit 3 */
            ((status & MCI_AR_MASK) >> 51);     /* bit 4 */
    return (char*)severities[index];
}

static char* decode_hierarchy_level(uint64_t status)
{
    static const char* hlevels[] = {
        "L0",
        "L1",
        "L2",
        "G"
    };
    return (char*)hlevels[status & 0x3];
}

static char* decode_address_mode(uint64_t status)
{
    static const char* amodes[] = {
        "SegmentOffset",
        "LinearAddress",
        "PhysicalAddress",
        "MemoryAddress"
    };
    uint64_t index = ((status & MCI_ADDR_MODE_MASK) >> 6);
    return (char*)amodes[index];
}

static char* decode_memory_io(uint64_t status)
{
    static const char* it[] = {
        "M",
        "reserved",
        "IO",
        "other_transaction"
    };
    uint64_t index = ((status & 0xC) >> 2) & 0x3;
    return (char*)it[index];
}

static char* decode_timeout(uint64_t status)
{
    static const char* timeout[] = {
        "NOTIMEOUT",
        "TIMEOUT"
    };
    uint64_t index = (((status & 0x100) >> 8) & 0x1);
    return (char*)timeout[index];
}

static char* decode_request_type(uint64_t status)
{
    static const char* request_types[] = {
        "ERR",
        "RD", /* Memory Read Error */
        "WR", /* Memory Write Error */
        "DRD",/* Address/Command Error */
        "DWR",/* Memory Scrubbing Error */
        "IRD",
        "PREFETCH",
        "EVICT",
        "SNOOP",
        "Unknown"
    };
    uint64_t index = (status & 0xF0) >> 4;
    if(8 < index) {
        index = 9;
    }
    return (char*)request_types[index];
}

static char* decode_participation(uint64_t status)
{
    static const char* part_types[] = {
        "SRC",
        "RES",
        "OBS",
        "generic"
    };
    uint64_t index = ((status & 0x600) >> 9);
    return (char*)part_types[index];
}

static char* decode_transaction_type(uint64_t status)
{
    static const char* tt[] = {
        "I", /* Instruction */
        "D", /* Data */
        "G", /* Generic */
        "Unknown"
    };
    uint64_t index = (status & 0xC) >> 2;
    return (char*)tt[index];
}

static char* decode_cache_health(uint64_t status)
{
    static const char* health[] = {
        "NotTracking",
        "Green",
        "Yellow",
        "Reserved"
    };
    uint64_t index = (status >> 53) & 0x3;
    return (char*)health[index];
}

static void mcedata_mem_ctrl_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    orcm_value_t *sensor_metric;
    bool miscv;
    uint64_t lsb_addr;
    bool correct_filter;
    uint32_t channel_num;
    char* str = NULL;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 2 - Memory Controller Errors");

    sensor_metric = orcm_util_load_orcm_value("error_type", "mem_ctrl_error", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    miscv = (mce_reg[MCI_STATUS] & MCI_MISCV_MASK)? true : false ;

    if (((mce_reg[MCG_CAP] & MCG_TES_P_MASK) == MCG_TES_P_MASK) &&
        ((mce_reg[MCG_CAP] & MCG_CMCI_P_MASK) == MCG_CMCI_P_MASK)) { /* Sec. 15.3.2.2.1 */
        str = decode_severity(mce_reg[MCG_CAP]);
        sensor_metric = orcm_util_load_orcm_value("error_severity", str, OPAL_STRING, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }

    str = decode_request_type(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("request_type", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Channel Number */
    if ((mce_reg[MCI_STATUS] & 0xF) != 0xf) {
        channel_num = (mce_reg[MCI_STATUS] & 0xF);
        sensor_metric = orcm_util_load_orcm_value("channel_number", &channel_num, OPAL_UINT, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }
    if(miscv  && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        str = decode_address_mode(mce_reg[MCI_MISC]);
        sensor_metric = orcm_util_load_orcm_value("address_mode", str, OPAL_STRING, NULL);

        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);

        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        sensor_metric = orcm_util_load_orcm_value("recov_addr_lsb", &lsb_addr, OPAL_UINT, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }

    if(mce_reg[MCI_STATUS]&0x1000) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "Corrected filtering enabled");
        correct_filter = true;
        sensor_metric = orcm_util_load_orcm_value("corrected_filtering", &correct_filter, OPAL_BOOL, NULL);
    } else {
        correct_filter = false;
        sensor_metric = orcm_util_load_orcm_value("corrected_filtering", &correct_filter, OPAL_BOOL, NULL);
    }
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);
}

void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    orcm_value_t *sensor_metric;
    bool addrv, miscv, uc, val;
    uint64_t mask, lsb_addr;
    bool correct_filter;
    char* str = NULL;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 3 - Cache Hierarchy Errors");

    val = (mce_reg[MCI_STATUS] & MCI_VALID_MASK)? true : false ;

    if (false == val) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCi_Status not valid: %lx", mce_reg[MCI_STATUS]);
        return;
    } else {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCi_status VALID");
    }

    sensor_metric = orcm_util_load_orcm_value("error_type", "cache_error", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Get the cache level */
    str = decode_hierarchy_level(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("hierarchy_level", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Transaction Type */
    str = decode_transaction_type(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("transaction_type", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Request Type */
    str = decode_request_type(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("request_type", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    val = (mce_reg[MCI_STATUS] & MCI_VALID_MASK)? true : false ;
    uc = (mce_reg[MCI_STATUS] & MCI_UC_MASK)? true : false ;
    miscv = (mce_reg[MCI_STATUS] & MCI_MISCV_MASK)? true : false ;
    addrv = (mce_reg[MCI_STATUS] & MCI_ADDRV_MASK)? true : false ;

    mask = (MCG_TES_P_MASK | MCG_CMCI_P_MASK);
    if (((mce_reg[MCG_CAP] & mask) == mask)) { /* Sec. 15.3.2.2.1 */
        str = decode_severity(mce_reg[MCI_STATUS]);
        sensor_metric = orcm_util_load_orcm_value("error_severity", str, OPAL_STRING, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);

        if(!uc && ((mce_reg[MCG_CAP]&0x800)>>11)) { /* Table 15-1 */
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                                "Enhanced chache reporting available");
            str = decode_cache_health(mce_reg[MCI_STATUS]);
            sensor_metric = orcm_util_load_orcm_value("cache_health", str, OPAL_STRING, NULL);
            ORCM_ON_NULL_RETURN(sensor_metric);
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        }
    }

    if(miscv && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        str = decode_address_mode(mce_reg[MCI_MISC]);
        sensor_metric = orcm_util_load_orcm_value("address_mode", str, OPAL_STRING, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);

        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        sensor_metric = orcm_util_load_orcm_value("recov_addr_lsb", &lsb_addr, OPAL_UINT, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }
    if (addrv) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "ADDR Register Valid");
        sensor_metric = orcm_util_load_orcm_value("err_addr", &mce_reg[MCI_ADDR], OPAL_UINT64, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }

    if(mce_reg[MCI_STATUS]&0x1000) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "Corrected filtering enabled");
        correct_filter = true;
        sensor_metric = orcm_util_load_orcm_value("corrected_filtering", &correct_filter, OPAL_BOOL, NULL);
    } else {
        correct_filter = false;
        sensor_metric = orcm_util_load_orcm_value("corrected_filtering", &correct_filter, OPAL_BOOL, NULL);
    }
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);
}

void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    orcm_value_t *sensor_metric;
    bool miscv;
    uint64_t lsb_addr;
    char* str = NULL;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 4 - Bus and Interconnect Errors");

    sensor_metric = orcm_util_load_orcm_value("error_type", "bus_ic_error", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Request Type */
    str = decode_request_type(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("request_type", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    str = decode_participation(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("participation", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Timeout */
    str = decode_timeout(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("T", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Memory/IO */
    str = decode_memory_io(mce_reg[MCI_STATUS]);
    sensor_metric = orcm_util_load_orcm_value("IT", str, OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    /* Get the level */
    str = decode_hierarchy_level(mce_reg[MCI_STATUS] & 0x3);
    sensor_metric = orcm_util_load_orcm_value("hierarchy_level", str,
                                              OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    miscv = ((mce_reg[MCI_STATUS] & MCI_MISCV_MASK) == MCI_MISCV_MASK)?true:false;

    if ((mce_reg[MCG_CAP] & (MCG_TES_P_MASK | MCG_CMCI_P_MASK)) == (MCG_TES_P_MASK | MCG_CMCI_P_MASK)) { /* Sec. 15.3.2.2.1 */
        str = decode_severity(mce_reg[MCI_STATUS]);
        sensor_metric = orcm_util_load_orcm_value("error_severity", str, OPAL_STRING, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }

    if(miscv && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        str = decode_address_mode(mce_reg[MCI_MISC]);
        sensor_metric = orcm_util_load_orcm_value("address_mode", str, OPAL_STRING, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);

        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        sensor_metric = orcm_util_load_orcm_value("recov_addr_lsb", &lsb_addr, OPAL_UINT, NULL);
        ORCM_ON_NULL_RETURN(sensor_metric);
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
    }
}

void mcedata_unknown_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    orcm_value_t *sensor_metric;
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 5 - Unknown Error");

    sensor_metric = orcm_util_load_orcm_value("ErrorLocation", "Unknown Error", OPAL_STRING, NULL);
    ORCM_ON_NULL_RETURN(sensor_metric);
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);
}

void mcedata_decode(unsigned long *mce_reg, opal_list_t *vals)
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
        case e_gen_cache_error :    mcedata_gen_cache_filter(mce_reg, vals);
                                    break;
        case e_tlb_error :          mcedata_tlb_filter(mce_reg, vals);
                                    break;
        case e_mem_ctrl_error :     mcedata_mem_ctrl_filter(mce_reg, vals);
                                    break;
        case e_cache_error :        mcedata_cache_filter(mce_reg, vals);
                                    break;
        case e_bus_ic_error :       mcedata_bus_ic_filter(mce_reg, vals);
                                    break;
        case e_unknown_error :      mcedata_unknown_filter(mce_reg, vals);
                                    break;
    }

}

static void mcedata_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor mcedata : mcedata_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_mcedata_component.use_progress_thread) {
       collect_mcedata_sample(sampler);
    }

}

/* Sample function looks for a standard template that mcelog uses to log the
 * errors when the 'raw' switch in mcelog's configuration file is enabled
 * The template it follows is:
 * CPU
 * BANK
 * TSC
 * RIP
 * MISC
 * ADDR
 * STATUS
 * MCGSTATUS
 * PROCESSOR
 * TIME
 * SOCKETID
 * APICID
 * MCGCAP
 *
 * This lines may be interspersed with other different log messages.
 */

void collect_mcedata_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    const char *temp = "mcedata";
    opal_buffer_t data, *bptr;
    time_t now;
    bool packed;
    uint64_t i = 0, cpu=0, socket=0, bank=0;
    uint64_t mce_reg[MCE_REG_COUNT];
    long tot_bytes;
    char* line;
    char *loc = NULL;
    struct timeval current_time;
    FILE *fp;
    void* metrics_obj = mca_sensor_mcedata_component.runtime_metrics;

    if(mca_sensor_mcedata_component.test) {
        generate_test_vector(sampler);
        return;
    }
    if(!orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor mcedata : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_mcedata_component.diagnostics |= 0x1;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "Logfile used: %s", mca_sensor_mcedata_component.logfile);

    fp = fopen(mca_sensor_mcedata_component.logfile, "r");

    if (NULL == fp) {
        return;
    }
    tot_bytes = update_log_file_size(fp);

    if (tot_bytes < 0){
      fclose(fp);
      return;
    }

    fseek(fp, log_file_pos, SEEK_SET);

    while (ftell(fp) < tot_bytes){

        get_log_lines(fp);

        if (NULL == log_lines[mcelog_cpu]){ // no more mce errors in log file
            break;
        }

        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
         "-------------------------------------------------------------------");

        line = log_lines[mcelog_cpu];
        loc = strstr(line, " CPU ");
        cpu = strtoull(loc+strlen(" CPU"), NULL, 16);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "CPU: %lx --- %s", cpu, line);

        line = log_lines[mcelog_bank];
        loc = strstr(line, " BANK ");
        bank = strtoull(loc+strlen(" BANK"), NULL, 16);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "BANK: %lx --- %s", bank, line);

        line = log_lines[mcelog_misc];
        loc = strstr(line, " MISC ");
        mce_reg[MCI_MISC] = strtoull(loc+strlen(" MISC"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCi_MISC: 0x%lx", mce_reg[MCI_MISC]);

        line = log_lines[mcelog_addr];
        loc = strstr(line, " ADDR ");
        mce_reg[MCI_ADDR] = strtoull(loc+strlen(" ADDR"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCi_ADDR: 0x%lx", mce_reg[MCI_ADDR]);

        line = log_lines[mcelog_status];
        loc = strstr(line, " STATUS ");
        mce_reg[MCI_STATUS] = strtoull(loc+strlen(" STATUS 0x"), NULL, 16);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCi_STATUS: 0x%lx", mce_reg[MCI_STATUS]);

        line = log_lines[mcelog_mcgstatus];
        loc = strstr(line, " MCGSTATUS ");
        mce_reg[MCG_STATUS] = strtoull(loc+strlen(" MCGSTATUS 0x"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCG_STATUS: 0x%lx", mce_reg[MCG_STATUS]);

        line = log_lines[mcelog_time];
        loc = strstr(line, " TIME ");
        now = strtoull(loc+strlen(" TIME"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "TIME: %lu", now);

        line = log_lines[mcelog_socketid];
        loc = strstr(line, " SOCKETID ");
        socket = strtoull(loc+strlen(" SOCKETID"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "SocketID: 0x%lx", socket);

        line = log_lines[mcelog_mcgcap];
        loc = strstr(line, " MCGCAP ");
        mce_reg[MCG_CAP] = strtoull(loc+strlen(" MCGCAP"), NULL, 0);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MCG_CAP: 0x%lx", mce_reg[MCG_CAP]);

        free_log_lines();

        /* prep to store the results */
        OBJ_CONSTRUCT(&data, opal_buffer_t);
        packed = true;

        /* pack our name */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &temp, 1, OPAL_STRING))) {
            fclose(fp);
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* store our hostname */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING))) {
            fclose(fp);
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        gettimeofday(&current_time, NULL);

        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
            fclose(fp);
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        while (i < 5) {
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "Packing %s : %lu",mce_reg_name[i], mce_reg[i]);
            /* store the mce register */
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &mce_reg[i], 1, OPAL_UINT64))) {
                fclose(fp);
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                return;
            }
            i++;
        }

        /* store the logical cpu ID */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &cpu, 1, OPAL_UINT))) {
            fclose(fp);
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
        /* Store the socket id */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &socket, 1, OPAL_UINT))) {
            fclose(fp);
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* xfer the data for transmission */
        if (packed) {
            bptr = &data;
            if (OPAL_SUCCESS != (ret = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
                fclose(fp);
                ORTE_ERROR_LOG(ret);
                OBJ_DESTRUCT(&data);
                return;
            }
        }
        i = 0;
        OBJ_DESTRUCT(&data);

    }

    log_file_pos = tot_bytes;
    fclose(fp);
}

static void mcedata_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    struct timeval sampletime;
    int rc;
    int32_t n, i = 0;
    uint64_t mce_reg[MCE_REG_COUNT];
    uint32_t cpu, socket;
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;


    analytics_vals = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);
    ORCM_ON_NULL_GOTO(analytics_vals, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->key, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->non_compute_data, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->compute_data, cleanup);

    /* unpack the host this came from */
    n=1;
    rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    /* sample time */
    n=1;
    rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received log from host %s with xx cores",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname);

    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(analytics_vals->non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(analytics_vals->key, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("data_group", "mcedata", OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(analytics_vals->key, (opal_list_item_t *)sensor_metric);

    while (i < 5) {
        /* MCE Registers */
        n=1;
        rc = opal_dss.unpack(sample, &mce_reg[i], &n, OPAL_UINT64);
        ORCM_ON_FAILURE_GOTO(rc, cleanup);
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                    "Collected %s: %lu", mce_reg_name[i],mce_reg[i]);

        sensor_metric = orcm_util_load_orcm_value((char *)mce_reg_name[i], &mce_reg[i], OPAL_UINT64, NULL);
        ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
        opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
        i++;
    }
    sensor_metric = NULL;
    /* Logical CPU ID */
    n=1;
    rc = opal_dss.unpack(sample, &cpu, &n, OPAL_UINT);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    /* Socket ID */
    n=1;
    rc = opal_dss.unpack(sample, &socket, &n, OPAL_UINT);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                "Collected logical CPU's ID %d: Socket ID %d", cpu, socket );

    sensor_metric = orcm_util_load_orcm_value("cpu", &cpu, OPAL_UINT, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("socket", &socket, OPAL_UINT, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)sensor_metric);
    sensor_metric = NULL;
    mcedata_decode(mce_reg, analytics_vals->compute_data);

    orcm_analytics.send_data(analytics_vals);

cleanup:
    SAFEFREE(hostname);
    ORCM_RELEASE(analytics_vals);
    ORCM_RELEASE(sensor_metric);
}

static void mcedata_set_sample_rate(int sample_rate)
{
    /* set the mcedata sample rate if seperate thread is enabled */
    if (mca_sensor_mcedata_component.use_progress_thread) {
        mca_sensor_mcedata_component.sample_rate = sample_rate;
    }
    return;
}

void mcedata_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if mcedata sample rate is provided for this*/
        *sample_rate = mca_sensor_mcedata_component.sample_rate;
    }
    return;
}

static void mcedata_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    const unsigned int tot_items = 23;
    static char *sensor_names[] = {
        "MCG_STATUS",
        "MCG_CAP",
        "MCI_STATUS",
        "MCI_ADDR",
        "MCI_MISC",
        "cpu",
        "socket",
        "ErrorLocation",
        "hierarchy_level",
        "error_location",
        "error_type",
        "error_severity",
        "request_type",
        "channel_number",
        "address_mode",
        "recov_addr_lsb",
        "corrected_filtering",
        "transaction_type",
        "cache_health",
        "err_addr",
        "participation",
        "T",
        "IT"
    };
    unsigned int i = 0;
    const char *comp ="mcedata";
    char *comp_name = NULL;
    int rc = OPAL_SUCCESS;
    struct timeval time_stamp;

    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ON_FAILURE_RETURN(rc);
    gettimeofday(&time_stamp, NULL);
    rc = opal_dss.pack(inventory_snapshot, &time_stamp, 1, OPAL_TIMEVAL);
    ON_FAILURE_RETURN(rc);

    for(i = 0; i < tot_items; ++i) {
        asprintf(&comp_name, "sensor_mcedata_%d", i+1);
        ON_NULL_RETURN(comp_name);
        rc = opal_dss.pack(inventory_snapshot, &comp_name, 1, OPAL_STRING);
        SAFEFREE(comp_name);
        ON_FAILURE_RETURN(rc);
        comp_name = sensor_names[i];
        rc = opal_dss.pack(inventory_snapshot, &comp_name, 1, OPAL_STRING);
        ON_FAILURE_RETURN(rc);
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    OBJ_RELEASE(kvs);
}

static void mcedata_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    unsigned int tot_items = 23;
    int n = 1;
    opal_list_t *records = NULL;
    int rc = OPAL_SUCCESS;
    orcm_value_t *time_stamp;
    struct timeval current_time;
    orcm_value_t *mkv = NULL;
    char *inv = NULL;
    char *inv_val = NULL;

    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);

    records = OBJ_NEW(opal_list_t);
    ON_NULL_RETURN(records);
    mkv = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
    ON_NULL_GOTO(mkv, cleanup);
    opal_list_append(records, (opal_list_item_t*)mkv);
    mkv = NULL; /* Now owned by records */
    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    ON_NULL_GOTO(time_stamp, cleanup);
    opal_list_append(records, (opal_list_item_t*)time_stamp);
    for(unsigned int i = 0; i < tot_items; ++i) {
        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, cleanup);
        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, cleanup);

        mkv = orcm_util_load_orcm_value(inv, inv_val, OPAL_STRING, NULL);
        SAFEFREE(inv);
        SAFEFREE(inv_val);
        ON_NULL_GOTO(mkv, cleanup);
        opal_list_append(records, (opal_list_item_t*)mkv);
        mkv = NULL; /* Now owned by records */
    }
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
        goto do_exit;
    }
cleanup:
    my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
    SAFEFREE(inv);
    SAFEFREE(inv_val);
do_exit:
    return;
}

int mcedata_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_mcedata_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int mcedata_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_mcedata_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int mcedata_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_mcedata_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}

static void generate_test_vector(orcm_sensor_sampler_t* sampler)
{
    int ret;
    opal_buffer_t data;
    const char *temp = "mcedata";
    struct timeval current_time;
    unsigned int uval = 0;
    opal_buffer_t* buffer = &data;

    mca_sensor_mcedata_component.diagnostics |= (mca_sensor_mcedata_component.collect_metrics)?1:0;
    OBJ_CONSTRUCT(&data, opal_buffer_t);

    ret = opal_dss.pack(buffer, &temp, 1, OPAL_STRING);
    ON_FAILURE_GOTO(ret, cleanup);

    ret = opal_dss.pack(buffer, &orcm_sensor_base.host_tag_value, 1, OPAL_STRING);
    ON_FAILURE_GOTO(ret, cleanup);

    gettimeofday(&current_time, NULL);
    ret = opal_dss.pack(buffer, &current_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_GOTO(ret, cleanup);

    for(int i = 0; i < 5; ++i) {
        uint64_t val = ((uint64_t)0xa5) << i;
        ret = opal_dss.pack(buffer, &val, 1, OPAL_UINT64);
        ON_FAILURE_GOTO(ret, cleanup);
    }
    uval = 1;
    ret = opal_dss.pack(buffer, &uval, 1, OPAL_UINT);
    ON_FAILURE_GOTO(ret, cleanup);
    uval = 1;
    ret = opal_dss.pack(buffer, &uval, 1, OPAL_UINT);
    ON_FAILURE_GOTO(ret, cleanup);

    ret = opal_dss.pack(&sampler->bucket, &buffer, 1, OPAL_BUFFER);
    ON_FAILURE_GOTO(ret, cleanup);

cleanup:
    OBJ_DESTRUCT(&data);
}

/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
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

#include "sensor_mcedata.h"


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
static void get_log_lines(FILE *fp);
static void perthread_mcedata_sample(int fd, short args, void *cbdata);
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void mcedata_set_sample_rate(int sample_rate);
static void mcedata_get_sample_rate(int *sample_rate);


/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_mcedata_module = {
    init,
    finalize,
    start,
    stop,
    mcedata_sample,
    mcedata_log,
    NULL,
    NULL,
    mcedata_set_sample_rate,
    mcedata_get_sample_rate
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
static char *log_lines[mcelog_sentinel];

static void start_log_file(void)
{
    FILE *fp;
    long tot_bytes = -1;
    long ret;
    log_file_pos = 0;
    char buf[128];

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
    long tot_bytes;
    int ret;
    static int errorReported = 0;
    char buf[128];

    if (fp){
        ret = fseek(fp, 0, SEEK_END);

        if (ret == 0){
          tot_bytes = ftell(fp);
        }
    }

    if (tot_bytes < 0){
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
    int i;
    for (i=0; i < mcelog_sentinel; i++){
        if (log_lines[i] != NULL){
          free(log_lines[i]);
          log_lines[i] = NULL;
        }
    }
}

static void get_log_lines(FILE *fp)
{
    int nextTag = 0;
    char *ptr=NULL;
    size_t n=0;

    while (nextTag < mcelog_sentinel && -1 != getline(&ptr, &n, fp)){
 
        if (strstr(ptr, "mcelog") && strstr(ptr, log_line_token[nextTag])){
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

    /* always construct this so we don't segfault in finalize */
    OBJ_CONSTRUCT(&tracking, opal_list_t);

    /*
     * Open up the base directory so we can get a listing
     */
    if (NULL == (cur_dirp = opendir("/dev"))) {
        OBJ_DESTRUCT(&tracking);
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
        if (0 == strcmp(dirname, "/dev/mcelog"))
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
    OPAL_LIST_DESTRUCT(&tracking);
    if(true == mce_default) {
        free(mca_sensor_mcedata_component.logfile);
        mca_sensor_mcedata_component.logfile = NULL;
    }
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
    collect_sample(sampler);
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

static void mcedata_gen_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 0 - Generic Cache Hierarchy Errors");

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
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 1 - TLB Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("error_location");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("tlb_Error");
    opal_list_append(vals, &kv->super);

}

static void mcedata_mem_ctrl_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    bool ar, s, pcc, addrv, miscv, en, uc, over, val;
    uint64_t addr_type, lsb_addr;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 2 - Memory Controller Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("error_type");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("mem_ctrl_error");
    opal_list_append(vals, &kv->super);

    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    over = (mce_reg[MCI_STATUS] & MCI_OVERFLOW_MASK)? true : false ;
    val = (mce_reg[MCI_STATUS] & MCI_VALID_MASK)? true : false ;
    uc = (mce_reg[MCI_STATUS] & MCI_UC_MASK)? true : false ;
    en = (mce_reg[MCI_STATUS] & MCI_EN_MASK)? true : false ;
    miscv = (mce_reg[MCI_STATUS] & MCI_MISCV_MASK)? true : false ;
    addrv = (mce_reg[MCI_STATUS] & MCI_ADDRV_MASK)? true : false ;
    pcc = (mce_reg[MCI_STATUS] & MCI_PCC_MASK)? true : false ;
    s = (mce_reg[MCI_STATUS] & MCI_S_MASK)? true : false ;
    ar = (mce_reg[MCI_STATUS] & MCI_AR_MASK)? true : false ;

    if (((mce_reg[MCG_CAP] & MCG_TES_P_MASK) == MCG_TES_P_MASK) &&
        ((mce_reg[MCG_CAP] & MCG_CMCI_P_MASK) == MCG_CMCI_P_MASK)) { /* Sec. 15.3.2.2.1 */
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("error_severity");
        kv->type = OPAL_STRING;
        if (uc && pcc) {
            kv->data.string = strdup("UC");
        } else if(!(uc || pcc)) {
            kv->data.string = strdup("CE");
        } else if (uc) {
            if (!(pcc || s || ar)) {
                kv->data.string = strdup("UCNA");
            } else if (!(pcc || (!s) || ar)) {
                kv->data.string = strdup("SRAO");
            } else if (!(pcc || (!s) || (!ar))) {
                kv->data.string = strdup("SRAR");
            } else {
                kv->data.string = strdup("UNKNOWN");
            }
        } else {
            kv->data.string = strdup("UNKNOWN");
        }
        opal_list_append(vals, &kv->super);
    }

    /* Request Type */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("request_type");
    kv->type = OPAL_STRING;
    switch ((mce_reg[MCI_STATUS] & 0x70) >> 4) {
        case 0: kv->data.string = strdup("GEN"); break;     /* Generic Undefined Request */
        case 1: kv->data.string = strdup("RD"); break;      /* Memory Read Error */
        case 2: kv->data.string = strdup("WR"); break;      /* Memory Write Error */
        case 3: kv->data.string = strdup("AC"); break;      /* Address/Command Error */
        case 4: kv->data.string = strdup("MS"); break;      /* Memory Scrubbing Error */
        default: kv->data.string = strdup("Reserved"); break; /* Reserved */
    }
    opal_list_append(vals, &kv->super);

    /* Channel Number */
    if ((mce_reg[MCI_STATUS] & 0xF) != 0xf) {
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("channel_number");
        kv->type = OPAL_UINT;
        kv->data.uint = (mce_reg[MCI_STATUS] & 0xF);
        opal_list_append(vals, &kv->super);
    }
    if(miscv  && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("address_mode");
        kv->type = OPAL_STRING;
        addr_type = ((mce_reg[MCI_MISC] & MCI_ADDR_MODE_MASK) >> 6);
        switch (addr_type) {
            case 0: kv->data.string = strdup("SegmentOffset"); break;
            case 1: kv->data.string = strdup("LinearAddress"); break;
            case 2: kv->data.string = strdup("PhysicalAddress"); break;
            case 3: kv->data.string = strdup("MemoryAddress"); break;
            case 7: kv->data.string = strdup("Generic"); break;
            default: kv->data.string = strdup("Reserved"); break;
        }
        opal_list_append(vals, &kv->super);
        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("recov_addr_lsb");
        kv->type = OPAL_UINT;
        kv->data.uint = lsb_addr;
        opal_list_append(vals, &kv->super);
    }

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("corrected_filtering");
    kv->type = OPAL_BOOL;

    if(mce_reg[MCI_STATUS]&0x1000) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "Corrected filtering enabled");
        kv->data.flag = true;
    } else {
        kv->data.flag = false;
    }
    opal_list_append(vals, &kv->super);
}

static void mcedata_cache_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    bool ar, s, pcc, addrv, miscv, en, uc, over, val;
    uint64_t cache_health, addr_type, lsb_addr;
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

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("error_type");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("cache_error");
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

    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    over = (mce_reg[MCI_STATUS] & MCI_OVERFLOW_MASK)? true : false ;
    val = (mce_reg[MCI_STATUS] & MCI_VALID_MASK)? true : false ;
    uc = (mce_reg[MCI_STATUS] & MCI_UC_MASK)? true : false ;
    en = (mce_reg[MCI_STATUS] & MCI_EN_MASK)? true : false ;
    miscv = (mce_reg[MCI_STATUS] & MCI_MISCV_MASK)? true : false ;
    addrv = (mce_reg[MCI_STATUS] & MCI_ADDRV_MASK)? true : false ;
    pcc = (mce_reg[MCI_STATUS] & MCI_PCC_MASK)? true : false ;
    s = (mce_reg[MCI_STATUS] & MCI_S_MASK)? true : false ;
    ar = (mce_reg[MCI_STATUS] & MCI_AR_MASK)? true : false ;

    if (((mce_reg[MCG_CAP] & MCG_TES_P_MASK) == MCG_TES_P_MASK) &&
        ((mce_reg[MCG_CAP] & MCG_CMCI_P_MASK) == MCG_CMCI_P_MASK)) { /* Sec. 15.3.2.2.1 */
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("error_severity");
        kv->type = OPAL_STRING;
        if (uc && pcc) {
            kv->data.string = strdup("UC");
        } else if(!(uc || pcc)) {
            kv->data.string = strdup("CE");
        } else if (uc) {
            if (!(pcc || s || ar)) {
                kv->data.string = strdup("UCNA");
            } else if (!(pcc || (!s) || ar)) {
                kv->data.string = strdup("SRAO");
            } else if (!(pcc || (!s) || (!ar))) {
                kv->data.string = strdup("SRAR");
            } else {
                kv->data.string = strdup("UNKNOWN");
            }
        } else {
            kv->data.string = strdup("UNKNOWN");
        }
        opal_list_append(vals, &kv->super);

        if(!uc && ((mce_reg[MCG_CAP]&0x800)>>11)) { /* Table 15-1 */
            opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                                "Enhanced chache reporting available");
            cache_health = (((mce_reg[MCI_STATUS] & HEALTH_STATUS_MASK ) >> 53) & 0x3);
            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("cache_health");
            kv->type = OPAL_STRING;

            switch (cache_health) {
                case 0: kv->data.string = strdup("NotTracking"); break;
                case 1: kv->data.string = strdup("Green"); break;
                case 2: kv->data.string = strdup("Yellow"); break;
                case 3: kv->data.string = strdup("Reserved"); break;
                default: kv->data.string = strdup("UNKNOWN"); break;
            }
            opal_list_append(vals, &kv->super);
        }
    }

    if(miscv && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("address_mode");
        kv->type = OPAL_STRING;
        addr_type = ((mce_reg[MCI_MISC] & MCI_ADDR_MODE_MASK) >> 6);
        switch (addr_type) {
            case 0: kv->data.string = strdup("SegmentOffset"); break;
            case 1: kv->data.string = strdup("LinearAddress"); break;
            case 2: kv->data.string = strdup("PhysicalAddress"); break;
            case 3: kv->data.string = strdup("MemoryAddress"); break;
            case 7: kv->data.string = strdup("Generic"); break;
            default: kv->data.string = strdup("Reserved"); break;
        }
        opal_list_append(vals, &kv->super);
        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("recov_addr_lsb");
        kv->type = OPAL_UINT;
        kv->data.uint = lsb_addr;
        opal_list_append(vals, &kv->super);
    }
    if (addrv) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "ADDR Register Valid");
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("err_addr");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = mce_reg[MCI_ADDR];
        opal_list_append(vals, &kv->super);
    }

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("corrected_filtering");
    kv->type = OPAL_BOOL;

    if(mce_reg[MCI_STATUS]&0x1000) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "Corrected filtering enabled");
        kv->data.flag = true;
    } else {
        kv->data.flag = true;
    }
        opal_list_append(vals, &kv->super);

}

static void mcedata_bus_ic_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    uint64_t pp, t, ii;
    bool ar, s, pcc, addrv, miscv, en, uc, over, val;
    uint64_t addr_type, lsb_addr;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 4 - Bus and Interconnect Errors");

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("error_type");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("bus_ic_error");
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

    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("participation");
    kv->type = OPAL_STRING;

    pp = ((mce_reg[MCI_STATUS] & 0x600) >> 9);
    switch (pp) {
        case 0: kv->data.string = strdup("SRC"); break;
        case 1: kv->data.string = strdup("RES"); break;
        case 2: kv->data.string = strdup("OBS"); break;
        case 3: kv->data.string = strdup("generic"); break;
    }
    opal_list_append(vals, &kv->super);

    /* Timeout */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("T");
    kv->type = OPAL_STRING;

    t = (((mce_reg[MCI_STATUS] & 0x100) >> 8) & 0x1);
    switch (t) {
        case 0: kv->data.string = strdup("NOTIMEOUT"); break;
        case 1: kv->data.string = strdup("TIMEOUT"); break;
    }
    opal_list_append(vals, &kv->super);

    /* Memory/IO */
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("II");
    kv->type = OPAL_STRING;

    ii = (((mce_reg[MCI_STATUS] & 0xC) >> 2)&0x3);
    switch (ii) {
        case 0: kv->data.string = strdup("M"); break;
        case 1: kv->data.string = strdup("reserved"); break;
        case 2: kv->data.string = strdup("IO"); break;
        case 3: kv->data.string = strdup("other_transaction"); break;
    }
    opal_list_append(vals, &kv->super);

    /* Get the level */
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

    /* Fig 15-6 of the Intel(R) 64 & IA-32 spec */
    over = (mce_reg[MCI_STATUS] & MCI_OVERFLOW_MASK)? true : false ;
    val = (mce_reg[MCI_STATUS] & MCI_VALID_MASK)? true : false ;
    uc = (mce_reg[MCI_STATUS] & MCI_UC_MASK)? true : false ;
    en = (mce_reg[MCI_STATUS] & MCI_EN_MASK)? true : false ;
    miscv = (mce_reg[MCI_STATUS] & MCI_MISCV_MASK)? true : false ;
    addrv = (mce_reg[MCI_STATUS] & MCI_ADDRV_MASK)? true : false ;
    pcc = (mce_reg[MCI_STATUS] & MCI_PCC_MASK)? true : false ;
    s = (mce_reg[MCI_STATUS] & MCI_S_MASK)? true : false ;
    ar = (mce_reg[MCI_STATUS] & MCI_AR_MASK)? true : false ;

    if (((mce_reg[MCG_CAP] & MCG_TES_P_MASK) == MCG_TES_P_MASK) &&
        ((mce_reg[MCG_CAP] & MCG_CMCI_P_MASK) == MCG_CMCI_P_MASK)) { /* Sec. 15.3.2.2.1 */
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("error_severity");
        kv->type = OPAL_STRING;
        if (uc && pcc) {
            kv->data.string = strdup("UC");
        } else if(!(uc || pcc)) {
            kv->data.string = strdup("CE");
        } else if (uc) {
            if (!(pcc || s || ar)) {
                kv->data.string = strdup("UCNA");
            } else if (!(pcc || (!s) || ar)) {
                kv->data.string = strdup("SRAO");
            } else if (!(pcc || (!s) || (!ar))) {
                kv->data.string = strdup("SRAR");
            } else {
                kv->data.string = strdup("UNKNOWN");
            }
        } else {
            kv->data.string = strdup("UNKNOWN");
        }
        opal_list_append(vals, &kv->super);
    }

    if(miscv && ((mce_reg[MCG_CAP] & MCG_SER_P_MASK) == MCG_SER_P_MASK)) {
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "MISC Register Valid");
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("address_mode");
        kv->type = OPAL_STRING;
        addr_type = ((mce_reg[MCI_MISC] & MCI_ADDR_MODE_MASK) >> 6);
        switch (addr_type) {
            case 0: kv->data.string = strdup("SegmentOffset"); break;
            case 1: kv->data.string = strdup("LinearAddress"); break;
            case 2: kv->data.string = strdup("PhysicalAddress"); break;
            case 3: kv->data.string = strdup("MemoryAddress"); break;
            case 7: kv->data.string = strdup("Generic"); break;
            default: kv->data.string = strdup("Reserved"); break;
        }
        opal_list_append(vals, &kv->super);
        lsb_addr = ((mce_reg[MCI_MISC] & MCI_RECV_ADDR_MASK));
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("recov_addr_lsb");
        kv->type = OPAL_UINT;
        kv->data.uint = lsb_addr;
        opal_list_append(vals, &kv->super);
    }
}

static void mcedata_unknown_filter(unsigned long *mce_reg, opal_list_t *vals)
{
    opal_value_t *kv;
    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "MCE Error Type 5 - Unknown Error");

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
       collect_sample(sampler);
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

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int ret;
    char *temp;
    opal_buffer_t data, *bptr;
    time_t now;
    bool packed;
    uint64_t i = 0, cpu=0, socket=0, bank=0, j;
    uint64_t mce_reg[MCE_REG_COUNT];
    long tot_bytes;
    char* line;
    char *loc = NULL;
    struct timeval current_time;
    FILE *fp;

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "Logfile used: %s", mca_sensor_mcedata_component.logfile);

    fp = fopen(mca_sensor_mcedata_component.logfile, "r");

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

        gettimeofday(&current_time, NULL);

        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }

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

        /* store the logical cpu ID */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &cpu, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
        }
        /* Store the socket id */
        if (OPAL_SUCCESS != (ret = opal_dss.pack(&data, &socket, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(ret);
            OBJ_DESTRUCT(&data);
            return;
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
        i = 0;
        OBJ_DESTRUCT(&data);

    }

    log_file_pos = tot_bytes;
    fclose(fp);
}

static void mycleanup(int dbhandle, int status, opal_list_t *kvs,
                      opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void mcedata_log(opal_buffer_t *sample)
{
    char *hostname=NULL;
    struct timeval sampletime;
    int rc;
    int32_t n, i = 0;
    opal_list_t *vals;
    opal_value_t *kv;
    uint64_t mce_reg[MCE_REG_COUNT];
    uint32_t cpu, socket;
    orcm_metric_value_t *sensor_metric;

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
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL))) {
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
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_TIMEVAL;
    kv->data.tv = sampletime;
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
    if (NULL == kv) {
        ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
        return;
    }
    kv->key = strdup("data_group");
    kv->type = OPAL_STRING;
    kv->data.string = strdup("mcedata");
    opal_list_append(vals, &kv->super);

    while (i < 5) {
        /* MCE Registers */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &mce_reg[i], &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                    "Collected %s: %lu", mce_reg_name[i],mce_reg[i]);
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup(mce_reg_name[i]);
        sensor_metric->value.type = OPAL_UINT64;
        sensor_metric->value.data.uint64 = mce_reg[i];
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        i++;
    }

    /* Logical CPU ID */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &cpu, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* Socket ID */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &socket, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                "Collected logical CPU's ID %d: Socket ID %d", cpu, socket );

    sensor_metric = OBJ_NEW(orcm_metric_value_t);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
        return;
    }
    sensor_metric->value.key = strdup("cpu");
    sensor_metric->value.type = OPAL_UINT;
    sensor_metric->value.data.uint = cpu;
    sensor_metric->units = NULL;
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    sensor_metric = OBJ_NEW(orcm_metric_value_t);
    if (NULL == sensor_metric) {
        ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
        return;
    }
    sensor_metric->value.key = strdup("socket");
    sensor_metric->value.type = OPAL_UINT;
    sensor_metric->value.data.uint = socket;
    sensor_metric->units = NULL;
    opal_list_append(vals, (opal_list_item_t *)sensor_metric);

    mcedata_decode(mce_reg, vals);


    /* store it */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_ENV_DATA, vals, NULL, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

 cleanup:
    if (NULL != hostname) {
        free(hostname);
    }

}

static void mcedata_set_sample_rate(int sample_rate)
{
    /* set the mcedata sample rate if seperate thread is enabled */
    if (mca_sensor_mcedata_component.use_progress_thread) {
        mca_sensor_mcedata_component.sample_rate = sample_rate;
    }
    return;
}

static void mcedata_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if mcedata sample rate is provided for this*/
        if (mca_sensor_mcedata_component.use_progress_thread) {
            *sample_rate = mca_sensor_mcedata_component.sample_rate;
        }
    }
    return;
}

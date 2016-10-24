/*
 * Copyright (c) 2013-2016 Intel Corporation. All rights reserved.
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
#include <math.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef ORCM_SIGAR_LINUX
#include <sigar.h>
#else
#include <libsigar-universal-macosx>
#endif

#include "opal_stdint.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_ring_buffer.h"
#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/mca/pstat/pstat.h"
#include "opal/mca/event/event.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/analytics/analytics.h"
#include "orcm/util/utils.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "sensor_sigar.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void sigar_sample(orcm_sensor_sampler_t *sampler);
static void perthread_sigar_sample(int fd, short args, void *cbdata);
void collect_sigar_sample(orcm_sensor_sampler_t *sampler);
static void sigar_log(opal_buffer_t *buf);
static void sigar_set_sample_rate(int sample_rate);
static void sigar_get_sample_rate(int *sample_rate);
static void sigar_inventory_collect(opal_buffer_t *inventory_snapshot);
static void sigar_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);

int sigar_enable_sampling(const char* sensor_specification);
int sigar_disable_sampling(const char* sensor_specification);
int sigar_reset_sampling(const char* sensor_specification);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_sigar_module = {
    init,
    finalize,
    start,
    stop,
    sigar_sample,
    sigar_log,
    sigar_inventory_collect,
    sigar_inventory_log,
    sigar_set_sample_rate,
    sigar_get_sample_rate,
    sigar_enable_sampling,
    sigar_disable_sampling,
    sigar_reset_sampling
};

/* define some local classes */
typedef struct {
    opal_list_item_t super;
    char *interface;
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t tx_packets;
    uint64_t tx_bytes;
} sensor_sigar_interface_t;
static void sit_cons(sensor_sigar_interface_t *sit)
{
    sit->interface = NULL;
    sit->rx_packets = 0;
    sit->rx_bytes = 0;
    sit->tx_packets = 0;
    sit->tx_bytes = 0;
}
static void sit_dest(sensor_sigar_interface_t *sit)
{
    if (NULL != sit->interface) {
        free(sit->interface);
    }
}
OBJ_CLASS_INSTANCE(sensor_sigar_interface_t,
                   opal_list_item_t,
                   sit_cons, sit_dest);

typedef struct {
    opal_list_item_t super;
    char *mount_pt;
    uint64_t reads;
    uint64_t writes;
    uint64_t read_bytes;
    uint64_t write_bytes;
} sensor_sigar_disks_t;
static void dit_cons(sensor_sigar_disks_t *dit)
{
    dit->mount_pt = NULL;
    dit->reads = 0;
    dit->writes = 0;
    dit->read_bytes = 0;
    dit->write_bytes = 0;
}
static void dit_dest(sensor_sigar_disks_t *dit)
{
    if (NULL != dit->mount_pt) {
        free(dit->mount_pt);
    }
}
OBJ_CLASS_INSTANCE(sensor_sigar_disks_t,
                   opal_list_item_t,
                   dit_cons, dit_dest);

static sigar_t *sigar;
static opal_list_t fslist;
static opal_list_t netlist;
static time_t last_sample = 0;
static struct cpu_data_t {
    uint64_t user;
    uint64_t nice;
    uint64_t sys;
    uint64_t idle;
    uint64_t wait;
    uint64_t total;
} pcpu;
static struct swap_data_t {
    uint64_t page_in;
    uint64_t page_out;
} pswap;

static orcm_sensor_sampler_t *sigar_sampler = NULL;
static orcm_sensor_sigar_t orcm_sensor_sigar;

static char* END_DG_MARKER = "END_OF_DATA_GROUPS";
static char* DATA_GROUP_KEY = "data_group";
static char* PLUGIN_NAME = "sigar";

static uint64_t metric_diff_calc(sigar_uint64_t newval, uint64_t oldval,
                                 const char *name_for_log,
                                 const char* value_name_for_log);

#define ON_NULL_RETURN(x) ORCM_ON_NULL_RETURN_ERROR(x,ORCM_ERR_OUT_OF_RESOURCE)
#define ON_NULL_GOTO(x,y) ORCM_ON_NULL_GOTO(x,y)
#define ON_FAILURE_GOTO(x,y) ORCM_ON_FAILURE_GOTO(x,y)
#define ON_NULL_PARAM_RETURN(x) if(NULL==x) return ORCM_ERR_BAD_PARAM

static int init(void)
{
    sigar_file_system_list_t sigar_fslist;
    sigar_net_interface_list_t sigar_netlist;
    sensor_sigar_disks_t *dit;
    sensor_sigar_interface_t *sit;
    unsigned int i;

    memset(&sigar_fslist, 0, sizeof(sigar_file_system_list_t));
    memset(&sigar_netlist, 0, sizeof(sigar_net_interface_list_t));

    mca_sensor_sigar_component.diagnostics = 0;
    mca_sensor_sigar_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create(PLUGIN_NAME, orcm_sensor_base.collect_metrics,
                                                mca_sensor_sigar_component.collect_metrics);

    /* setup the globals */
    OBJ_CONSTRUCT(&fslist, opal_list_t);
    OBJ_CONSTRUCT(&netlist, opal_list_t);
    pcpu.user = 0;
    pcpu.nice = 0;
    pcpu.sys = 0;
    pcpu.idle = 0;
    pcpu.wait = 0;
    pcpu.total = 0;
    pswap.page_in = 0;
    pswap.page_out = 0;

    /* initialize sigar */
    if(!mca_sensor_sigar_component.test) {
        if (0 != sigar_open(&sigar)) {
            opal_output(0, "%s: sigar_open failed on node %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        orte_process_info.nodename);
            return ORTE_ERROR;
        }
    }

    /* load the disk list */
    if(mca_sensor_sigar_component.test) {
        sigar_fslist.number = 1;
        sigar_fslist.size = sizeof(sigar_file_system_t);
        sigar_fslist.data = malloc(sizeof(sigar_file_system_t));
        ON_NULL_GOTO(sigar_fslist.data, error_exit);
        sigar_file_system_t* fs = sigar_fslist.data;
        strcpy(fs->dir_name, "/");
        strcpy(fs->dev_name, "/dev/sda1");
        strcpy(fs->type_name, "local");
        strcpy(fs->sys_type_name, "ext3");
        strcpy(fs->options, "");
        fs->type = SIGAR_FSTYPE_LOCAL_DISK;
        fs->flags = 0;
    } else {
        if (0 != sigar_file_system_list_get(sigar, &sigar_fslist)) {
            return ORTE_ERROR;
        }
    }
    for (i = 0; i < sigar_fslist.number; i++) {
        if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK || sigar_fslist.data[i].type == SIGAR_FSTYPE_NETWORK) {
            dit = OBJ_NEW(sensor_sigar_disks_t);
            dit->mount_pt = strdup(sigar_fslist.data[i].dir_name);
            opal_list_append(&fslist, &dit->super);
        }
    }
    if(mca_sensor_sigar_component.test) {
        SAFEFREE(sigar_fslist.data);
    } else {
        sigar_file_system_list_destroy(sigar, &sigar_fslist);
    }

    /* load the list of network interfaces */
    if(mca_sensor_sigar_component.test) {
        sigar_netlist.number = 1;
        sigar_netlist.size = 0;
        sigar_netlist.data = malloc(sizeof(char*));
        ON_NULL_GOTO(sigar_netlist.data, error_exit);
        sigar_netlist.data[0] = "eth0";
    } else {
        if (0 != sigar_net_interface_list_get(sigar, &sigar_netlist)) {
            return ORCM_ERROR;
        }
    }
    for (i=0; i < sigar_netlist.number; i++) {
        sit = OBJ_NEW(sensor_sigar_interface_t);
        sit->interface = strdup(sigar_netlist.data[i]);
        opal_list_append(&netlist, &sit->super);
    }
    if(mca_sensor_sigar_component.test) {
        SAFEFREE(sigar_netlist.data);
    } else {
        sigar_net_interface_list_destroy(sigar, &sigar_netlist);
    }

    return ORCM_SUCCESS;

error_exit:
    return ORCM_ERR_OUT_OF_RESOURCE;
}

static void finalize(void)
{
    opal_list_item_t *item;

    if (NULL != sigar && !mca_sensor_sigar_component.test) {
        sigar_close(sigar);
    }
    while (NULL != (item = opal_list_remove_first(&fslist))) {
        ORCM_RELEASE(item);
    }
    OBJ_DESTRUCT(&fslist);
    while (NULL != (item = opal_list_remove_first(&netlist))) {
        ORCM_RELEASE(item);
    }
    OBJ_DESTRUCT(&netlist);

    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_sigar_component.runtime_metrics);
    mca_sensor_sigar_component.runtime_metrics = NULL;

    return;
}

/*
 * Start monitoring of local processes
 */
static void start(orte_jobid_t jobid)
{
    /* start a separate sigar progress thread for sampling */
    if (mca_sensor_sigar_component.use_progress_thread) {
        if (!orcm_sensor_sigar.ev_active) {
            orcm_sensor_sigar.ev_active = true;
            if (NULL == (orcm_sensor_sigar.ev_base = opal_progress_thread_init(PLUGIN_NAME))) {
                orcm_sensor_sigar.ev_active = false;
                return;
            }
        }

        /* setup sigar sampler */
        sigar_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if sigar sample rate is provided for this*/
        if (!mca_sensor_sigar_component.sample_rate) {
            mca_sensor_sigar_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        sigar_sampler->rate.tv_sec = mca_sensor_sigar_component.sample_rate;
        sigar_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_sigar.ev_base, &sigar_sampler->ev,
                               perthread_sigar_sample, sigar_sampler);
        opal_event_evtimer_add(&sigar_sampler->ev, &sigar_sampler->rate);
    }else{
	 mca_sensor_sigar_component.sample_rate = orcm_sensor_base.sample_rate;

    }

    return;
}

static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_sigar.ev_active) {
        orcm_sensor_sigar.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause(PLUGIN_NAME);
    }
    return;
}

static int sigar_collect_mem(opal_list_t* data)
{
    int rc;
    sigar_mem_t mem;
    /* get the memory usage for this node */
    memset(&mem, 0, sizeof(mem));
    if(mca_sensor_sigar_component.test) {
        mem.total       = 0x1000000000;
        mem.used        = 0x0010000000;
        mem.actual_used = 0x000C000000;
        mem.actual_free = 0x0FD0000000;
    } else {
        if (SIGAR_OK != (rc = sigar_mem_get(sigar, &mem))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_mem_get failed: %s", error_string);
        }
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "mem total: %" PRIu64 " used: %" PRIu64 " actual used: %" PRIu64 " actual free: %" PRIu64 "",
                        (uint64_t)mem.total, (uint64_t)mem.used, (uint64_t)mem.actual_used, (uint64_t)mem.actual_free);

    /* add it to the data */
    orcm_value_t* item = NULL;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "mem_total")) {
        item = orcm_util_load_orcm_value("mem_total", &mem.total, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "mem_used")) {
        item = orcm_util_load_orcm_value("mem_used", &mem.used, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "mem_actual_used")) {
        item = orcm_util_load_orcm_value("mem_actual_used", &mem.actual_used, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "mem_actual_free")) {
        item = orcm_util_load_orcm_value("mem_actual_free", &mem.actual_free, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_swap(opal_list_t* data)
{
    int rc;
    sigar_swap_t swap;
    /* get swap data */
    memset(&swap, 0, sizeof(swap));
    if(mca_sensor_sigar_component.test) {
        swap.total    = 0x00100000;
        swap.used     = 0x00010000;
        swap.page_in  = 0x01000000;
        swap.page_out = 0x02000000;
    } else {
        if (SIGAR_OK != (rc = sigar_swap_get(sigar, &swap))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_swap_get failed: %s", error_string);
        }
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "swap total: %" PRIu64 " used: %" PRIu64 "page_in: %" PRIu64 " page_out: %" PRIu64 "\n",
                        (uint64_t)swap.total, (uint64_t)swap.used, (uint64_t)swap.page_in, (uint64_t)swap.page_out);

    /* add it to the data */
    orcm_value_t* item = NULL;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "swap_total")) {
        item = orcm_util_load_orcm_value("swap_total", &swap.total, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "swap_used")) {
        item = orcm_util_load_orcm_value("swap_used", &swap.used, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    uint64_t ui64 = swap.page_in - pswap.page_in;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "swap_page_in")) {
        item = orcm_util_load_orcm_value("swap_page_in", &ui64, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    ui64 = swap.page_out - pswap.page_out;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "swap_page_out")) {
        item = orcm_util_load_orcm_value("swap_page_out", &ui64, OPAL_UINT64, "Bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_cpu(opal_list_t* data)
{
    int rc;
    sigar_cpu_t cpu;
    double cpu_diff;
    float tmp;
    /* get the cpu usage */
    memset(&cpu, 0, sizeof(cpu));
    if(mca_sensor_sigar_component.test) {
        cpu.total    = 25;
        cpu.user     = 5;
        cpu.nice     = 2;
        cpu.stolen   = 0;
        cpu.sys      = 7;
        cpu.soft_irq = 2;
        cpu.wait     = 7;
        cpu.irq      = 2;
        cpu.idle     = 75;
    } else {
        if (SIGAR_OK != (rc = sigar_cpu_get(sigar, &cpu))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_cpu_get failed: %s", error_string);

        }
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "cpu user: %" PRIu64 " sys: %" PRIu64 " idle: %" PRIu64 " wait: %" PRIu64 " nice: %" PRIu64 " total: %" PRIu64 "",
            (uint64_t)cpu.user, (uint64_t)cpu.sys, (uint64_t)cpu.idle, (uint64_t)cpu.wait, (uint64_t)cpu.nice, (uint64_t)cpu.total);

    /* add it to the data */
    orcm_value_t* item = NULL;
    cpu_diff = (double)(cpu.total - pcpu.total);
    tmp = (float)((cpu.user - pcpu.user) * 100.0 / cpu_diff) + (float)((cpu.nice - pcpu.nice) * 100.0 / cpu_diff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "cpu_user")) {
        item = orcm_util_load_orcm_value("cpu_user", &tmp, OPAL_FLOAT, "%");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    tmp = ((float) (cpu.sys - pcpu.sys) * 100.0 / cpu_diff) + ((float)((cpu.wait - pcpu.wait) * 100.0 / cpu_diff));
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "cpu_sys")) {
        item = orcm_util_load_orcm_value("cpu_sys", &tmp, OPAL_FLOAT, "%");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    tmp = (float) (cpu.idle - pcpu.idle) * 100.0 / cpu_diff;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "cpu_idle")) {
        item = orcm_util_load_orcm_value("cpu_idle", &tmp, OPAL_FLOAT, "%");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    /* update the values */
    pcpu.user = cpu.user;
    pcpu.nice = cpu.nice;
    pcpu.sys = cpu.sys;
    pcpu.wait = cpu.wait;
    pcpu.idle = cpu.idle;
    pcpu.total = cpu.total;

    return ORCM_SUCCESS;
}

static int sigar_collect_load(opal_list_t* data)
{
    int rc;
    sigar_loadavg_t loadavg;
    float tmp;

    /* get load average data */
    memset(&loadavg, 0, sizeof(loadavg));
    if(mca_sensor_sigar_component.test) {
        loadavg.loadavg[0] = 0.5;
        loadavg.loadavg[1] = 0.4;
        loadavg.loadavg[2] = 0.3;
    } else {
        if (SIGAR_OK != (rc = sigar_loadavg_get(sigar, &loadavg))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_loadavg_get failed: %s", error_string);
        }
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "load_avg: %e %e %e",
                        loadavg.loadavg[0], loadavg.loadavg[1], loadavg.loadavg[2]);
    /* add it to the data */
    tmp = (float)loadavg.loadavg[0];
    orcm_value_t* item = NULL;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "load0")) {
        item = orcm_util_load_orcm_value("load0", &tmp, OPAL_FLOAT, "%/100");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    tmp = (float)loadavg.loadavg[1];
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "load1")) {
        item = orcm_util_load_orcm_value("load1", &tmp, OPAL_FLOAT, "%/100");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    tmp = (float)loadavg.loadavg[2];
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "load2")) {
        item = orcm_util_load_orcm_value("load2", &tmp, OPAL_FLOAT, "%/100");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_disk(opal_list_t* data, double tdiff)
{
    int rc;
    sigar_disk_usage_t tdisk;
    sensor_sigar_disks_t *dit;
    sigar_file_system_usage_t fsusage;
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_read_bytes = 0;
    uint64_t total_write_bytes = 0;

    /* get disk usage data */
    memset(&tdisk, 0, sizeof(tdisk));
    OPAL_LIST_FOREACH(dit, &fslist, sensor_sigar_disks_t) {
        if(mca_sensor_sigar_component.test) {
            rc = SIGAR_OK;
            fsusage.use_percent = 0.5;
            fsusage.total = 0x8000000000;
            fsusage.free  = 0x6000000000;
            fsusage.used  = 0x2000000000;
            fsusage.avail = 0x1C00000000;
            fsusage.files = 15000;
            fsusage.free_files = 2500;
            fsusage.disk.reads = 200000;
            fsusage.disk.writes = 20000;
            fsusage.disk.write_bytes = 0x0007F12345;
            fsusage.disk.read_bytes = 0x007F123450;
            fsusage.disk.rtime = 23000;
            fsusage.disk.wtime = 5000;
            fsusage.disk.qtime = 27000;
            fsusage.disk.time = 0;
            fsusage.disk.snaptime = 0;
            fsusage.disk.service_time = 0.0;
            fsusage.disk.queue = 0.0;
        } else {
            rc = sigar_file_system_usage_get(sigar, dit->mount_pt, &fsusage);
        }
        if (SIGAR_OK != rc) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_file_system_usage_get failed: %s", error_string);
            opal_output(0, "%s Failed to get usage data for filesystem %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), dit->mount_pt);
        } else {
            opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                                "FileSystem: %s Reads: %" PRIu64 " Writes: %" PRIu64 " ReadBytes: %" PRIu64 " WriteBytes: %" PRIu64 "",
                                dit->mount_pt, (uint64_t)fsusage.disk.reads, (uint64_t)fsusage.disk.writes, (uint64_t)fsusage.disk.read_bytes, (uint64_t)fsusage.disk.write_bytes);
            /* compute the number of reads since last reading */
            reads = metric_diff_calc(fsusage.disk.reads, dit->reads, dit->mount_pt, "disk reads");
            dit->reads = fsusage.disk.reads;  /* old = new */
            /* compute the number of writes since last reading */
            writes = metric_diff_calc(fsusage.disk.writes, dit->writes, dit->mount_pt, "disk writes");
            dit->writes = fsusage.disk.writes;  /* old = new */
            /* compute the number of read bytes since last reading */
            read_bytes = metric_diff_calc(fsusage.disk.read_bytes, dit->read_bytes, dit->mount_pt, "disk read bytes");
            dit->read_bytes = fsusage.disk.read_bytes;  /* old = new */
            /* compute the number of bytes written since last reading */
            write_bytes = metric_diff_calc(fsusage.disk.write_bytes, dit->write_bytes, dit->mount_pt, "disk write bytes");
            dit->write_bytes = fsusage.disk.write_bytes;  /* old = new */
            opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                                "FileSystem: %s ReadsChange: %" PRIu64 " WritesChange: %" PRIu64 " ReadBytesChange: %" PRIu64 " WriteBytesChange: %" PRIu64 "",
                                dit->mount_pt, (uint64_t)reads, (uint64_t)writes, (uint64_t)read_bytes, (uint64_t)write_bytes);
            /* accumulate the values */
            tdisk.reads += reads;
            tdisk.writes += writes;
            tdisk.read_bytes += read_bytes;
            tdisk.write_bytes += write_bytes;
            tdisk.rtime += fsusage.disk.rtime;
            tdisk.wtime += fsusage.disk.wtime;
            tdisk.qtime += fsusage.disk.qtime;
            total_reads += fsusage.disk.reads;
            total_writes += fsusage.disk.writes;
            total_read_bytes += fsusage.disk.read_bytes;
            total_write_bytes += fsusage.disk.write_bytes;
        }
    }

    opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                        "Totals: ReadsChange: %" PRIu64 " WritesChange: %" PRIu64 " ReadBytesChange: %" PRIu64 " WriteBytesChange: %" PRIu64 "",
                        (uint64_t)tdisk.reads, (uint64_t)tdisk.writes, (uint64_t)tdisk.read_bytes, (uint64_t)tdisk.write_bytes);
    opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                        "Totals: ReadTime: %" PRIu64 " WriteTime: %" PRIu64 " ioTime: %" PRIu64 "",
                        (uint64_t)tdisk.rtime, (uint64_t)tdisk.wtime, (uint64_t)tdisk.qtime);

    /* add it to the data */
    orcm_value_t* item = NULL;
    uint64_t value = (uint64_t)ceil((double)tdisk.reads/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_ro_rate")) {
        item = orcm_util_load_orcm_value("disk_ro_rate", &value, OPAL_UINT64, "ops/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.writes/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_wo_rate")) {
        item = orcm_util_load_orcm_value("disk_wo_rate", &value, OPAL_UINT64, "ops/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.read_bytes/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_rb_rate")) {
        item = orcm_util_load_orcm_value("disk_rb_rate", &value, OPAL_UINT64, "bytes/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.write_bytes/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_wb_rate")) {
        item = orcm_util_load_orcm_value("disk_wb_rate", &value, OPAL_UINT64, "bytes/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_ro_total")) {
        item = orcm_util_load_orcm_value("disk_ro_total", &total_reads, OPAL_UINT64, "ops");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_wo_total")) {
        item = orcm_util_load_orcm_value("disk_wo_total", &total_writes, OPAL_UINT64, "ops");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_rb_total")) {
        item = orcm_util_load_orcm_value("disk_rb_total", &total_read_bytes, OPAL_UINT64, "bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_wb_total")) {
        item = orcm_util_load_orcm_value("disk_wb_total", &total_write_bytes, OPAL_UINT64, "bytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.rtime);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_rt_total")) {
        item = orcm_util_load_orcm_value("disk_rt_total", &value, OPAL_UINT64, "msec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.wtime);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_wt_total")) {
        item = orcm_util_load_orcm_value("disk_wt_total", &value, OPAL_UINT64, "msec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tdisk.qtime);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "disk_iot_total")) {
        item = orcm_util_load_orcm_value("disk_iot_total", &value, OPAL_UINT64, "msec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_network(opal_list_t *data, double tdiff)
{
    int rc;
    sensor_sigar_interface_t* sit = NULL;
    sensor_sigar_interface_t* next = NULL;
    sigar_net_interface_stat_t tnet;
    sigar_net_interface_stat_t ifc;
    uint64_t rxpkts = 0;
    uint64_t txpkts = 0;
    uint64_t rxbytes = 0;
    uint64_t txbytes = 0;
    uint64_t total_mbytes_sent = 0;
    uint64_t total_mbytes_recv = 0;
    uint64_t total_packets_sent = 0;
    uint64_t total_packets_recv = 0;
    uint64_t total_errors_sent = 0;
    uint64_t total_errors_recv = 0;

    /* get network usage data */
    memset(&tnet, 0, sizeof(tnet));
    OPAL_LIST_FOREACH_SAFE(sit, next, &netlist, sensor_sigar_interface_t) {
        memset(&ifc, 0, sizeof(ifc));
        if(mca_sensor_sigar_component.test) {
            rc = SIGAR_OK;
            ifc.rx_packets = 2000;
            ifc.rx_dropped = 0;
            ifc.rx_errors = 0;
            ifc.rx_bytes = ifc.rx_packets * 800;
            ifc.rx_frame = 0;
            ifc.rx_overruns = 0;
            ifc.speed = 0;
            ifc.tx_packets = 200000;
            ifc.tx_dropped = 0;
            ifc.tx_errors = 0;
            ifc.tx_bytes = ifc.tx_packets * 1200;
            ifc.tx_overruns = 0;
            ifc.tx_carrier = 0;
            ifc.tx_collisions = 0;
        } else {
            rc = sigar_net_interface_stat_get(sigar, sit->interface, &ifc);
        }
        if (SIGAR_OK != rc) {
            char* error_string = strerror(rc);
            opal_output_verbose(1 , orcm_sensor_base_framework.framework_output,
                                "sigar_net_interface_stat_get failed: %s", error_string);
            opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                                "%s Failed to get usage data for interface %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), sit->interface);
            /* if we failed to get stats on this interface, remove from the list
             * so that we stop trying in the future as well */
            opal_list_remove_item(&netlist, &sit->super);
            ORCM_RELEASE(sit);
        } else {
            opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                                "Interface: %s RecvdPackets: %" PRIu64 " RecvdBytes: %" PRIu64 " TransPackets: %" PRIu64 " TransBytes: %" PRIu64 "",
                                sit->interface, (uint64_t)ifc.rx_packets, (uint64_t)ifc.rx_bytes, (uint64_t)ifc.tx_packets, (uint64_t)ifc.tx_bytes);
            /* compute the number of recvd packets since last reading */
            rxpkts = metric_diff_calc(ifc.rx_packets, sit->rx_packets, sit->interface, "rx packets");
            sit->rx_packets = ifc.rx_packets;  /* old = new */
            /* compute the number of transmitted packets since last reading */
            txpkts = metric_diff_calc(ifc.tx_packets, sit->tx_packets, sit->interface, "tx packets");
            sit->tx_packets = ifc.tx_packets;  /* old = new */
            /* compute the number of recvd bytes since last reading */
            rxbytes = metric_diff_calc(ifc.rx_bytes, sit->rx_bytes, sit->interface, "rx bytes");
            sit->rx_bytes = ifc.rx_bytes;  /* old = new */
            /* compute the number of transmitted bytes since last reading */
            txbytes = metric_diff_calc(ifc.tx_bytes, sit->tx_bytes, sit->interface, "tx bytes");
            sit->tx_bytes = ifc.tx_bytes;  /* old = new */
            opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                                "Interface: %s RxPkts: %" PRIu64 " TxPkts: %" PRIu64 " RxBytes: %" PRIu64 " TxBytes: %" PRIu64 "",
                                sit->interface, (uint64_t)rxpkts, (uint64_t)txpkts, (uint64_t)rxbytes, (uint64_t)txbytes);
            /* accumulate the values */
            tnet.rx_packets += rxpkts;
            tnet.rx_bytes += rxbytes;
            tnet.tx_packets += txpkts;
            tnet.tx_bytes += txbytes;
            total_mbytes_sent += ifc.tx_bytes;
            total_mbytes_recv += ifc.rx_bytes;
            total_packets_sent += ifc.tx_packets;
            total_packets_recv += ifc.rx_packets;
            total_errors_sent += ifc.tx_errors;
            total_errors_recv += ifc.rx_errors;
        }
    }
    opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                        "Totals: RxPkts: %" PRIu64 " TxPkts: %" PRIu64 " RxBytes: %" PRIu64 " TxBytes: %" PRIu64 "",
                        (uint64_t)tnet.rx_packets, (uint64_t)tnet.tx_packets, (uint64_t)tnet.rx_bytes, (uint64_t)tnet.tx_bytes);

    /* add it to the data */
    orcm_value_t* item = NULL;
    uint64_t value = (uint64_t)ceil((double)tnet.rx_packets/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_rp_rate")) {
        item = orcm_util_load_orcm_value("net_rp_rate", &value, OPAL_UINT64, "packets/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tnet.tx_packets/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_wp_rate")) {
        item = orcm_util_load_orcm_value("net_wp_rate", &value, OPAL_UINT64, "packets/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tnet.rx_bytes/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_rb_rate")) {
        item = orcm_util_load_orcm_value("net_rb_rate", &value, OPAL_UINT64, "bytes/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)tnet.tx_bytes/tdiff);
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_wb_rate")) {
        item = orcm_util_load_orcm_value("net_wb_rate", &value, OPAL_UINT64, "bytes/sec");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)total_mbytes_sent/(1024*1024)); /* convert to Mbytes */
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_wb_total")) {
        item = orcm_util_load_orcm_value("net_wb_total", &value, OPAL_UINT64, "Mbytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    value = (uint64_t)ceil((double)total_mbytes_recv/(1024*1024)); /* convert to Mbytes */
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_rb_total")) {
        item = orcm_util_load_orcm_value("net_rb_total", &value, OPAL_UINT64, "Mbytes");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_wp_total")) {
        item = orcm_util_load_orcm_value("net_wp_total", &total_packets_sent, OPAL_UINT64, "packets");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_rp_total")) {
        item = orcm_util_load_orcm_value("net_rp_total", &total_packets_recv, OPAL_UINT64, "packets");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_tx_errors")) {
        item = orcm_util_load_orcm_value("net_tx_errors", &total_errors_sent, OPAL_UINT64, "packets");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "net_rx_errors")) {
        item = orcm_util_load_orcm_value("net_rx_errors", &total_errors_recv, OPAL_UINT64, "packets");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_system(opal_list_t* data)
{
    int rc;
    sigar_uptime_t utime;

    /* get load average data */
    memset(&utime, 0, sizeof(utime));
    if(mca_sensor_sigar_component.test) {
        utime.uptime = 16800.0;
    } else {
        if (SIGAR_OK != (rc = sigar_uptime_get(sigar, &utime))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_uptime_get failed: %s", error_string);
        }
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "uptime: %f", utime.uptime);

    /* add them to the data */
    orcm_value_t* item = NULL;
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "uptime")) {
        item = orcm_util_load_orcm_value("uptime", &utime.uptime, OPAL_DOUBLE, "seconds");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_global_procstat(opal_list_t* data)
{
    int rc;
    sigar_proc_stat_t       procstat_info;

    /* get load average data */
    memset(&procstat_info, 0, sizeof(procstat_info));
    if(mca_sensor_sigar_component.test) {
        procstat_info.idle = 0;
        procstat_info.running = 1;
        procstat_info.sleeping = 0;
        procstat_info.stopped = 0;
        procstat_info.threads = 24;
        procstat_info.zombie = 0;
        procstat_info.total = 1;
    } else {
        if (SIGAR_OK != (rc = sigar_proc_stat_get(sigar, &procstat_info))) {
            char* error_string = strerror(rc);
            opal_output(0, "sigar_proc_stat_get failed: %s", error_string);
        }
    }
    /* add them to the data */
    orcm_value_t* item = NULL;
    item = orcm_util_load_orcm_value(DATA_GROUP_KEY, "procstat", OPAL_STRING, "");
    ON_NULL_RETURN(item);
    opal_list_append(data, (opal_list_item_t*)item);

    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "total_processes")) {
        item = orcm_util_load_orcm_value("total_processes", &procstat_info.total, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "sleeping_processes")) {
        item = orcm_util_load_orcm_value("sleeping_processes", &procstat_info.sleeping, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "running_processes")) {
        item = orcm_util_load_orcm_value("running_processes", &procstat_info.running, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "zombie_processes")) {
        item = orcm_util_load_orcm_value("zombie_processes", &procstat_info.zombie, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "stopped_processes")) {
        item = orcm_util_load_orcm_value("stopped_processes", &procstat_info.stopped, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "idle_processes")) {
        item = orcm_util_load_orcm_value("idle_processes", &procstat_info.idle, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "total_threads")) {
        item = orcm_util_load_orcm_value("total_threads", &procstat_info.threads, OPAL_UINT64, "");
        ON_NULL_RETURN(item);
        opal_list_append(data, (opal_list_item_t*)item);
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_procstat(opal_list_t* data)
{
    int rc;
    sigar_proc_state_t proc_info;
    sigar_proc_mem_t   proc_mem_info;
    sigar_proc_cpu_t   proc_cpu_info;
    char *error_string;
    opal_pstats_t *stats = NULL;
    pid_t sample_pid;
    int i;
    int remaining_procs;
    orte_proc_t *child;

    if (NULL != orte_local_children && !mca_sensor_sigar_component.test) {
        remaining_procs = orte_local_children->size+1;
    } else {
        remaining_procs = 1;
    }

    for (i=0; i < remaining_procs; i++) {
        if(0==i) {
            sample_pid = orte_process_info.pid;
        } else {
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
            sample_pid = child->pid;
        }

        /* reset procstat data */
        memset(&proc_info, 0, sizeof(proc_info));
        memset(&proc_mem_info, 0, sizeof(proc_mem_info));
        memset(&proc_cpu_info, 0, sizeof(proc_cpu_info));

        /* Sample all the proc details here */
        bool log_group = true;
        if(mca_sensor_sigar_component.test) {
            strcpy(proc_info.name, "test_orcmd");
            proc_info.ppid = 14634;
            proc_info.nice = 0;
            proc_info.priority = 1;
            proc_info.processor = 0;
            proc_info.state = 'R';
            proc_info.threads = 24;
            proc_info.tty = 0;

            proc_mem_info.major_faults = 0;
            proc_mem_info.minor_faults = 3;
            proc_mem_info.page_faults = 23;
            proc_mem_info.resident = 120000000;
            proc_mem_info.share = 300000;
            proc_mem_info.size = 130000000;

            proc_cpu_info.last_time = 0;
            proc_cpu_info.percent = 0.22;
            proc_cpu_info.sys = 10;
            proc_cpu_info.user = 90;
            proc_cpu_info.total = 100;
            proc_cpu_info.start_time = 0;
        } else {
            if (SIGAR_OK != (rc = sigar_proc_state_get(sigar, sample_pid, &proc_info))) {
                error_string = strerror(rc);
                opal_output(0, "sigar_proc_state_get failed: %s", error_string);
                log_group = false;
            }
            if (SIGAR_OK != (rc = sigar_proc_mem_get(sigar, sample_pid, &proc_mem_info))) {
                error_string = strerror(rc);
                opal_output(0, "sigar_proc_mem_get failed: %s", error_string);
                log_group = false;
            }
            if (SIGAR_OK != (rc = sigar_proc_cpu_get(sigar, sample_pid, &proc_cpu_info))) {
                error_string = strerror(rc);
                opal_output(0, "sigar_loadavg_get failed: %s", error_string);
                log_group = false;
            }
        }
        if (false == log_group)
            continue;

        stats = OBJ_NEW(opal_pstats_t);

        stats->pid = orte_process_info.pid;
        strncpy(stats->cmd, proc_info.name, sizeof(stats->cmd)-1);
        stats->cmd[sizeof(stats->cmd)-1] = '\0';
        stats->state[0] = proc_info.state;
        stats->state[1] = '\0';
        stats->priority = proc_info.priority;
        stats->num_threads = proc_info.threads;
        stats->vsize = proc_mem_info.size;
        stats->rss = proc_mem_info.resident;
        stats->processor = proc_info.processor;
        stats->rank = 0;

        /* Store the data_group for this process */
        char* group = NULL;
        asprintf(&group, "procstat_%s", stats->cmd);
        if(NULL == group) {
            ORCM_RELEASE(stats);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        orcm_value_t* item = orcm_util_load_orcm_value(DATA_GROUP_KEY, group, OPAL_STRING, "");
        SAFEFREE(group);
        if(NULL == item) {
            ORCM_RELEASE(stats);
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        opal_list_append(data, (opal_list_item_t*)item);

        /* Store the data for this process */
        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "pid")) {
            item = orcm_util_load_orcm_value("pid", &stats->pid, OPAL_PID, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "state_1")) {
            item = orcm_util_load_orcm_value("state_1", &stats->state[0], OPAL_INT8, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "state_2")) {
            item = orcm_util_load_orcm_value("state_2", &stats->state[1], OPAL_INT8, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "percent_cpu")) {
            item = orcm_util_load_orcm_value("percent_cpu", &stats->percent_cpu, OPAL_FLOAT, "%");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "priority")) {
            item = orcm_util_load_orcm_value("priority", &stats->priority, OPAL_INT32, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "num_threads")) {
            item = orcm_util_load_orcm_value("num_threads", &stats->num_threads, OPAL_INT16, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "vsize")) {
            item = orcm_util_load_orcm_value("vsize", &stats->vsize, OPAL_FLOAT, "bytes");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "rss")) {
            item = orcm_util_load_orcm_value("rss", &stats->rss, OPAL_FLOAT, "bytes");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "processor")) {
            item = orcm_util_load_orcm_value("processor", &stats->processor, OPAL_INT16, "");
            if(NULL == item) {
                ORCM_RELEASE(stats);
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        ORCM_RELEASE(stats);

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "shared_memory")) {
            item = orcm_util_load_orcm_value("shared_memory", &proc_mem_info.share, OPAL_INT64, "bytes");
            if(NULL == item) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "minor_faults")) {
            item = orcm_util_load_orcm_value("minor_faults", &proc_mem_info.minor_faults, OPAL_INT64, "");
            if(NULL == item) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "major_faults")) {
            item = orcm_util_load_orcm_value("major_faults", &proc_mem_info.major_faults, OPAL_INT64, "");
            if(NULL == item) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "page_faults")) {
            item = orcm_util_load_orcm_value("page_faults", &proc_mem_info.page_faults, OPAL_INT64, "");
            if(NULL == item) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }

        if(orcm_sensor_base_runtime_metrics_do_collect(mca_sensor_sigar_component.runtime_metrics, "percent")) {
            item = orcm_util_load_orcm_value("percent", &proc_cpu_info.percent, OPAL_DOUBLE, "%");
            if(NULL == item) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
            opal_list_append(data, (opal_list_item_t*)item);
        }
    }

    return ORCM_SUCCESS;
}

static void sigar_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor sigar : sigar_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_sigar_component.use_progress_thread) {
       collect_sigar_sample(sampler);
    }

}

static void perthread_sigar_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor sigar : perthread_sigar_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_sigar_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if sigar sample rate is provided for this*/
    if (mca_sensor_sigar_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_sigar_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

void collect_sigar_sample(orcm_sensor_sampler_t *sampler)
{
    int rc;
    time_t now;
    double tdiff;
    void* metrics_obj = mca_sensor_sigar_component.runtime_metrics;

    if(0 == orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj) &&
       !orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor sigar : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_sigar_component.diagnostics |= 0x1;

    /* get the sample time */
    now = time(NULL);
    tdiff = difftime(now, last_sample);

    orcm_value_t* item = orcm_util_load_orcm_value(DATA_GROUP_KEY, PLUGIN_NAME, OPAL_STRING, "");
    if(NULL == item) {
        return;
    }
    opal_list_t collected_data;
    OBJ_CONSTRUCT(&collected_data, opal_list_t);
    opal_list_append(&collected_data, (opal_list_item_t*)item);

    /* 1. Memory stats*/
    if(mca_sensor_sigar_component.mem) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_mem(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 2. Swap Memory stats */
    if(mca_sensor_sigar_component.swap) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_swap(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 3. CPU stats*/
    if(mca_sensor_sigar_component.cpu) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_cpu(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 4. load stats */
    if(mca_sensor_sigar_component.load) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_load(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 5. disk stats */
    if(mca_sensor_sigar_component.disk) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_disk(&collected_data, tdiff))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 6. network stats */
    if(mca_sensor_sigar_component.network) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_network(&collected_data, tdiff))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 7. System stats */
    if(mca_sensor_sigar_component.sys) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_system(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }

    /* 8. proc stats */
    if(mca_sensor_sigar_component.proc) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_global_procstat(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_procstat(&collected_data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&collected_data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    }
    item = orcm_util_load_orcm_value(DATA_GROUP_KEY, END_DG_MARKER, OPAL_STRING, "");
    if(NULL == item) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        OBJ_DESTRUCT(&collected_data);
        return;
    }
    opal_list_append(&collected_data, (opal_list_item_t*)item);
    /* prep the buffer to pack the data */
    opal_buffer_t data;
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    if(ORCM_SUCCESS != (rc = orcm_sensor_pack_data_header(&data, PLUGIN_NAME, orte_process_info.nodename, &current_time))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        OBJ_DESTRUCT(&collected_data);
        return;
    }

    OPAL_LIST_FOREACH(item, &collected_data, orcm_value_t) {
        rc = orcm_sensor_pack_orcm_value(&data, item);
        if(ORCM_SUCCESS != rc) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            OBJ_DESTRUCT(&collected_data);
            return;
        }
    }
    OBJ_DESTRUCT(&collected_data);

    /* xfer the data for transmission - need at least one prior sample before doing so */
    if (0 < last_sample) {
        opal_buffer_t* bptr = &data;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    OBJ_DESTRUCT(&data);
    last_sample = now;
}

static void sigar_log(opal_buffer_t *sample)
{
    int rc;
    char* host = NULL;
    struct timeval sampletime;
    orcm_value_t* item = NULL;
    orcm_value_t* group = NULL;
    orcm_value_t* ctime = NULL;
    orcm_value_t* hostname = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;
    char* data_group = NULL;

    rc = orcm_sensor_unpack_data_header_from_plugin(sample, &host, &sampletime);

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received log from host %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == host) ? "NULL" : host);

    /* sample time */
    ctime = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, "");
    ON_NULL_GOTO(ctime,cleanup);

    /* hostname */
    hostname = orcm_util_load_orcm_value("hostname", host, OPAL_STRING, "");
    ON_NULL_GOTO(hostname,cleanup);

    /* Get first datagroup from sample */
    rc = orcm_sensor_unpack_orcm_value(sample, &group);
    if(ORCM_SUCCESS != rc) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    if(0 != strncmp(group->value.key, DATA_GROUP_KEY, strlen(DATA_GROUP_KEY))) {
        ORTE_ERROR_LOG(ORCM_ERR_UNPACK_FAILURE);
        goto cleanup;
    }
    data_group = strdup(group->value.data.string);

    /* Create first analytics structure */
    analytics_vals = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);
    ON_NULL_GOTO(analytics_vals, cleanup);
    ON_NULL_GOTO(analytics_vals->key, cleanup);
    ON_NULL_GOTO(analytics_vals->non_compute_data, cleanup);
    ON_NULL_GOTO(analytics_vals->compute_data, cleanup);

    opal_list_append(analytics_vals->non_compute_data, (opal_list_item_t*)ctime);
    ctime = NULL;
    opal_list_append(analytics_vals->key, (opal_list_item_t*)hostname);
    hostname = NULL;
    opal_list_append(analytics_vals->key, (opal_list_item_t*)group);
    group = NULL;

    while(0 != strncmp(END_DG_MARKER, data_group, strlen(END_DG_MARKER))) {
        /* Get next item */
        rc = orcm_sensor_unpack_orcm_value(sample, &item);
        ON_FAILURE_GOTO(rc,cleanup);
        if(0 == strncmp(item->value.key, DATA_GROUP_KEY, strlen(DATA_GROUP_KEY))) {
            group = item;
            item = NULL;
            SAFEFREE(data_group);
            data_group = strdup(group->value.data.string);
            /* Send to analytics and cleanup */
            if(0 < opal_list_get_size(analytics_vals->compute_data)) {
                orcm_analytics.send_data(analytics_vals);
            }
            ORCM_RELEASE(analytics_vals);

            /* Recreate key with new data group */
            ctime = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, "");
            ON_NULL_GOTO(ctime,cleanup);
            hostname = orcm_util_load_orcm_value("hostname", host, OPAL_STRING, NULL);
            ON_NULL_GOTO(hostname, cleanup);

            /* recreate analytics object for new data group */
            analytics_vals = orcm_util_load_orcm_analytics_value(NULL, NULL, NULL);
            ON_NULL_GOTO(analytics_vals, cleanup);
            ON_NULL_GOTO(analytics_vals->key, cleanup);
            ON_NULL_GOTO(analytics_vals->non_compute_data, cleanup);
            ON_NULL_GOTO(analytics_vals->compute_data, cleanup);
            opal_list_append(analytics_vals->non_compute_data, (opal_list_item_t*)ctime);
            ctime = NULL;
            opal_list_append(analytics_vals->key, (opal_list_item_t*)hostname);
            hostname = NULL;
            opal_list_append(analytics_vals->key, (opal_list_item_t*)group);
            group = NULL;
            continue;
        } else {
            opal_list_append(analytics_vals->compute_data, (opal_list_item_t*)item);
            item = NULL; /* owned by list */
        }
    }

cleanup:
    ORCM_RELEASE(analytics_vals);
    ORCM_RELEASE(group);
    ORCM_RELEASE(item);
    ORCM_RELEASE(hostname);
    ORCM_RELEASE(ctime);
    SAFEFREE(host);
    SAFEFREE(data_group);
}

/* Helper function to calculate the metric differences */
static uint64_t metric_diff_calc(sigar_uint64_t newval, uint64_t oldval,
                                 const char *name_for_log,
                                 const char *value_name_for_log)
{
    uint64_t diff;

    if (newval < oldval) {
        /* assume that the value was reset and we are starting over */
        opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                            "%s metric_diff_calc: new value %" PRIu64 " is less than old value %" PRIu64
                            " for %s metric %s; assume the value was reset and set diff to new value.",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            (uint64_t)newval, (uint64_t)oldval, name_for_log, value_name_for_log);
        diff = newval;
    } else {
        diff = newval - oldval;
    }

    return diff;
}

static void sigar_set_sample_rate(int sample_rate)
{
    /* set the sigar sample rate if seperate thread is enabled */
    if (mca_sensor_sigar_component.use_progress_thread) {
        mca_sensor_sigar_component.sample_rate = sample_rate;
    }
    return;
}

static void sigar_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if sigar sample rate is provided for this*/
            *sample_rate = mca_sensor_sigar_component.sample_rate;
    }
    return;
}

static void sigar_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    static char* sensor_names[] = {
      /* primary_key=sigar (36) */
        "mem_total",
        "mem_used",
        "mem_actual_used",
        "mem_actual_free",
        "swap_total",
        "swap_used",
        "swap_page_in",
        "swap_page_out",
        "cpu_user",
        "cpu_sys",
        "cpu_idle",
        "load0",
        "load1",
        "load2",
        "disk_ro_rate",
        "disk_wo_rate",
        "disk_rb_rate",
        "disk_wb_rate",
        "disk_ro_total",
        "disk_wo_total",
        "disk_rb_total",
        "disk_wb_total",
        "disk_rt_total",
        "disk_wt_total",
        "disk_iot_total",
        "net_rp_rate",
        "net_wp_rate",
        "net_rb_rate",
        "net_wb_rate",
        "net_wb_total",
        "net_rb_total",
        "net_wp_total",
        "net_rp_total",
        "net_tx_errors",
        "net_rx_errors",
        "uptime",
      /* primary_key=procstat (7) */
        "total_processes",
        "sleeping_processes",
        "running_processes",
        "zombie_processes",
        "stopped_processes",
        "idle_processes",
        "total_threads",
      /* primary_key=procstat_<process> (14) */
        "pid",
        "cmd",
        "state",
        "percent_cpu",
        "priority",
        "num_threads",
        "vsize",
        "rss",
        "processor",
        "shared_memory",
        "minor_faults",
        "major_faults",
        "page_faults",
        "percent"
    };
    unsigned int tot_items = 58;
    unsigned int i = 0;
    char *comp = NULL;
    int rc = OPAL_SUCCESS;

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &PLUGIN_NAME, 1, OPAL_STRING))) {
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
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    for(i = 0; i < tot_items; ++i) {
        asprintf(&comp, "sensor_sigar_%d", i+1);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            free(comp);
            return;
        }
        free(comp);
        orcm_sensor_base_runtime_metrics_track(mca_sensor_sigar_component.runtime_metrics, sensor_names[i]);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &sensor_names[i], 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
    }
}

static void my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    ORCM_RELEASE(kvs);
}

static void sigar_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
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
            ORCM_RELEASE(records);
            return;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            SAFEFREE(inv);
            ORCM_RELEASE(records);
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

int sigar_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_sigar_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int sigar_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_sigar_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int sigar_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_sigar_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);
}

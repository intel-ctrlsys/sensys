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

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_sigar.h"

#define MAX_PROC_NAME 35
/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void sigar_sample(orcm_sensor_sampler_t *sampler);
static void sigar_log(opal_buffer_t *buf);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_sigar_module = {
    init,
    finalize,
    start,
    stop,
    sigar_sample,
    sigar_log
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
static bool log_enabled = true;
static opal_buffer_t test_vector;

static uint64_t metric_diff_calc(sigar_uint64_t newval, uint64_t oldval,
                                 const char *name_for_log,
                                 const char* value_name_for_log);
static void generate_test_vector(opal_buffer_t *v);

static int init(void)
{
    sigar_file_system_list_t sigar_fslist;
    sigar_net_interface_list_t sigar_netlist;
    sensor_sigar_disks_t *dit;
    sensor_sigar_interface_t *sit;
    unsigned int i;

    if (mca_sensor_sigar_component.test) {
        /* generate test vector */
        OBJ_CONSTRUCT(&test_vector, opal_buffer_t);
        generate_test_vector(&test_vector);
        return ORCM_SUCCESS;
    }

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
    if (0 != sigar_open(&sigar)) {
        opal_output(0, "%s: sigar_open failed on node %s",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    orte_process_info.nodename);
        return ORTE_ERROR;
    }

    /* load the disk list */
    if (0 != sigar_file_system_list_get(sigar, &sigar_fslist)) {
        return ORTE_ERROR;
    }
    for (i = 0; i < sigar_fslist.number; i++) {
        if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK || sigar_fslist.data[i].type == SIGAR_FSTYPE_NETWORK) {
            dit = OBJ_NEW(sensor_sigar_disks_t);
            dit->mount_pt = strdup(sigar_fslist.data[i].dir_name);
            opal_list_append(&fslist, &dit->super);
        }
    }
    sigar_file_system_list_destroy(sigar, &sigar_fslist);

    /* load the list of network interfaces */
    if (0 != sigar_net_interface_list_get(sigar, &sigar_netlist)) {
        return ORTE_ERROR;
    }
    for (i=0; i < sigar_netlist.number; i++) {
        sit = OBJ_NEW(sensor_sigar_interface_t);
        sit->interface = strdup(sigar_netlist.data[i]);
        opal_list_append(&netlist, &sit->super);
    }
    sigar_net_interface_list_destroy(sigar, &sigar_netlist);

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    opal_list_item_t *item;

    if (mca_sensor_sigar_component.test) {
        /* destruct test vector */
        OBJ_DESTRUCT(&test_vector);
        return;
    }

    if (NULL != sigar) {
        sigar_close(sigar);
    }
    while (NULL != (item = opal_list_remove_first(&fslist))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&fslist);
    while (NULL != (item = opal_list_remove_first(&netlist))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&netlist);

    return;
}

/*
 * Start monitoring of local processes
 */
static void start(orte_jobid_t jobid)
{
    return;
}


static void stop(orte_jobid_t jobid)
{
    return;
}

static int sigar_collect_mem(opal_buffer_t *dataptr)
{
    int rc;
    sigar_mem_t mem;
    char *error_string;
    bool log_group = true;
    /* get the memory usage for this node */
    memset(&mem, 0, sizeof(mem));
    if (SIGAR_OK != (rc = sigar_mem_get(sigar, &mem))) {
        error_string = strerror(rc);
        opal_output(0, "sigar_mem_get failed: %s", error_string);
        log_group = false;
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "mem total: %" PRIu64 " used: %" PRIu64 " actual used: %" PRIu64 " actual free: %" PRIu64 "",
                        (uint64_t)mem.total, (uint64_t)mem.used, (uint64_t)mem.actual_used, (uint64_t)mem.actual_free);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;

    /* add it to the dataptr */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &mem.total, 1, OPAL_UINT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &mem.used, 1, OPAL_UINT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &mem.actual_used, 1, OPAL_UINT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &mem.actual_free, 1, OPAL_UINT64))) {
        return rc;
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_swap(opal_buffer_t *dataptr)
{
    int rc;
    sigar_swap_t swap;
    char *error_string;
    uint64_t ui64;
    bool log_group = true;
    /* get swap data */
    memset(&swap, 0, sizeof(swap));
    if (SIGAR_OK != (rc = sigar_swap_get(sigar, &swap))) {
        error_string = strerror(rc);
        opal_output(0, "sigar_swap_get failed: %s", error_string);
        log_group = false;
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "swap total: %" PRIu64 " used: %" PRIu64 "page_in: %" PRIu64 " page_out: %" PRIu64 "\n",
                        (uint64_t)swap.total, (uint64_t)swap.used, (uint64_t)swap.page_in, (uint64_t)swap.page_out);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;
    /* compute the values we actually want and add them to the dataptr */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &swap.total, 1, OPAL_UINT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &swap.used, 1, OPAL_UINT64))) {
        return rc;
    }
    ui64 = swap.page_in - pswap.page_in;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &ui64, 1, OPAL_UINT64))) {
        return rc;
    }
    ui64 = swap.page_out - pswap.page_out;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &ui64, 1, OPAL_UINT64))) {
        return rc;
    }
    return ORCM_SUCCESS;

}

static int sigar_collect_cpu(opal_buffer_t *dataptr)
{
    int rc;
    sigar_cpu_t cpu;
    char *error_string;
    double cpu_diff;
    float tmp;
    bool log_group = true;
    /* get the cpu usage */
    memset(&cpu, 0, sizeof(cpu));
    if (SIGAR_OK != (rc = sigar_cpu_get(sigar, &cpu))) {
        error_string = strerror(rc);
        opal_output(0, "sigar_cpu_get failed: %s", error_string);
        log_group = false;

    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "cpu user: %" PRIu64 " sys: %" PRIu64 " idle: %" PRIu64 " wait: %" PRIu64 " nice: %" PRIu64 " total: %" PRIu64 "", 
            (uint64_t)cpu.user, (uint64_t)cpu.sys, (uint64_t)cpu.idle, (uint64_t)cpu.wait, (uint64_t)cpu.nice, (uint64_t)cpu.total);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;

    /* compute the values we actually want and add them to the dataptr */
    cpu_diff = (double)(cpu.total - pcpu.total);
    tmp = (float)((cpu.user - pcpu.user) * 100.0 / cpu_diff) + (float)((cpu.nice - pcpu.nice) * 100.0 / cpu_diff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
    }
    tmp = ((float) (cpu.sys - pcpu.sys) * 100.0 / cpu_diff) + ((float)((cpu.wait - pcpu.wait) * 100.0 / cpu_diff));
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
    }
    tmp = (float) (cpu.idle - pcpu.idle) * 100.0 / cpu_diff;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
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

static int sigar_collect_load(opal_buffer_t *dataptr)
{
    int rc;
    sigar_loadavg_t loadavg;
    char *error_string;
    float tmp;
    bool log_group = true;

    /* get load average data */
    memset(&loadavg, 0, sizeof(loadavg));
    if (SIGAR_OK != (rc = sigar_loadavg_get(sigar, &loadavg))) {
        error_string = strerror(rc);
        opal_output(0, "sigar_loadavg_get failed: %s", error_string);
        log_group = false;
    }
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "load_avg: %e %e %e",
                        loadavg.loadavg[0], loadavg.loadavg[1], loadavg.loadavg[2]);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;
    /* add them to the dataptr */
    tmp = (float)loadavg.loadavg[0];
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
    }
    tmp = (float)loadavg.loadavg[1];
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
    }
    tmp = (float)loadavg.loadavg[2];
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &tmp, 1, OPAL_FLOAT))) {
        return rc;
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_disk(opal_buffer_t *dataptr, double tdiff)
{
    int rc;
    sigar_disk_usage_t tdisk;
    sensor_sigar_disks_t *dit;
    sigar_file_system_usage_t fsusage;
    uint64_t reads, writes, read_bytes, write_bytes;
    char *error_string;
    bool log_group = true;

    /* get disk usage data */
    memset(&tdisk, 0, sizeof(tdisk));
    OPAL_LIST_FOREACH(dit, &fslist, sensor_sigar_disks_t) {
        if (SIGAR_OK != (rc = sigar_file_system_usage_get(sigar, dit->mount_pt, &fsusage))) {
            error_string = strerror(rc);
            opal_output(0, "sigar_file_system_usage_get failed: %s", error_string);
            opal_output(0, "%s Failed to get usage data for filesystem %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), dit->mount_pt);
            log_group = false;
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
        }
    }
    opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                        "Totals: ReadsChange: %" PRIu64 " WritesChange: %" PRIu64 " ReadBytesChange: %" PRIu64 " WriteBytesChange: %" PRIu64 "", 
                        (uint64_t)tdisk.reads, (uint64_t)tdisk.writes, (uint64_t)tdisk.read_bytes, (uint64_t)tdisk.write_bytes);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;

    /* compute the values we actually want and add them to the dataptr */
    reads = (uint64_t)ceil((double)tdisk.reads/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &reads, 1, OPAL_UINT64))) {
        return rc;
    }
    writes = (uint64_t)ceil((double)tdisk.writes/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &writes, 1, OPAL_UINT64))) {
        return rc;
    }
    read_bytes = (uint64_t)ceil((double)tdisk.read_bytes/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &read_bytes, 1, OPAL_UINT64))) {
        return rc;
    }
    write_bytes = (uint64_t)ceil((double)tdisk.write_bytes/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &write_bytes, 1, OPAL_UINT64))) {
        return rc;
    }
    return ORCM_SUCCESS;
}

static int sigar_collect_network(opal_buffer_t *dataptr, double tdiff)
{
    int rc;
    sensor_sigar_interface_t *sit, *next;
    sigar_net_interface_stat_t tnet, ifc;
    uint64_t rxpkts, txpkts, rxbytes, txbytes;
    char *error_string;
    bool log_group = true;

    /* get network usage data */
    memset(&tnet, 0, sizeof(tnet));
    OPAL_LIST_FOREACH_SAFE(sit, next, &netlist, sensor_sigar_interface_t) {
        memset(&ifc, 0, sizeof(ifc));
        if (SIGAR_OK != (rc = sigar_net_interface_stat_get(sigar, sit->interface, &ifc))) {
            error_string = strerror(rc);
            opal_output_verbose(1 , orcm_sensor_base_framework.framework_output,
                                "sigar_net_interface_stat_get failed: %s", error_string);
            opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                                "%s Failed to get usage data for interface %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), sit->interface);
            /* if we failed to get stats on this interface, remove from the list
             * so that we stop trying in the future as well */
            opal_list_remove_item(&netlist, &sit->super);
            OBJ_RELEASE(sit);
            log_group = false;
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
        }
    }
    opal_output_verbose(4, orcm_sensor_base_framework.framework_output,
                        "Totals: RxPkts: %" PRIu64 " TxPkts: %" PRIu64 " RxBytes: %" PRIu64 " TxBytes: %" PRIu64 "", 
                        (uint64_t)tnet.rx_packets, (uint64_t)tnet.tx_packets, (uint64_t)tnet.rx_bytes, (uint64_t)tnet.tx_bytes);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;

    /* compute the values we actually want and add them to the data */
    rxpkts = (uint64_t)ceil((double)tnet.rx_packets/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &rxpkts, 1, OPAL_UINT64))) {
        return rc;
    }
    txpkts = (uint64_t)ceil((double)tnet.tx_packets/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &txpkts, 1, OPAL_UINT64))) {
        return rc;
    }
    rxbytes = (uint64_t)ceil((double)tnet.rx_bytes/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &rxbytes, 1, OPAL_UINT64))) {
        return rc;
    }
    txbytes = (uint64_t)ceil((double)tnet.tx_bytes/tdiff);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &txbytes, 1, OPAL_UINT64))) {
        return rc;
    }    
    return ORCM_SUCCESS;
}

static int sigar_collect_global_procstat(opal_buffer_t *dataptr)
{
    int rc;
    sigar_proc_stat_t       procstat_info;
    char *error_string;
    bool log_group = true;

    /* get load average data */
    memset(&procstat_info, 0, sizeof(procstat_info));
    if (SIGAR_OK != (rc = sigar_proc_stat_get(sigar, &procstat_info))) {
        error_string = strerror(rc);
        opal_output(0, "sigar_loadavg_get failed: %s", error_string);
        log_group = false;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
        return rc;
    }
    if (false == log_group)
        return ORCM_ERR_SENSOR_READ_FAIL;


    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.total, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.sleeping, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.running, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.zombie, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.stopped, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.idle, 1, OPAL_INT64))) {
        return rc;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &procstat_info.threads, 1, OPAL_INT64))) {
        return rc;
    }

    return ORCM_SUCCESS;
}

static int sigar_collect_procstat(opal_buffer_t *dataptr)
{
    int rc;
    sigar_proc_state_t proc_info;
    sigar_proc_mem_t   proc_mem_info;
    sigar_proc_cpu_t   proc_cpu_info;
    char *error_string;
    bool log_group = true;
    opal_pstats_t *stats;
    pid_t sample_pid;
    int i,remaining_procs;
    orte_proc_t *child;

    if (NULL != orte_local_children) {
        remaining_procs = orte_local_children->size+1;
    } else {
        remaining_procs = 1;
    }

    for (i=0; i < remaining_procs; i++) {
        if(0==i)
        {
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
        if (false == log_group)
            continue;

        stats = OBJ_NEW(opal_pstats_t);

        stats->pid = orte_process_info.pid;
        strncpy(stats->cmd, proc_info.name, sizeof(stats->cmd));
        stats->state[0] = proc_info.state;
        stats->state[1] = '\0';
        stats->priority = proc_info.priority;
        stats->num_threads = proc_info.threads;
        stats->vsize = proc_mem_info.size;
        stats->rss = proc_mem_info.resident;
        stats->processor = proc_info.processor;
        stats->rank = 0;

        log_group = true;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &log_group, 1, OPAL_BOOL))) {
            OBJ_DESTRUCT(stats);
            return rc;
        }

        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &stats, 1, OPAL_PSTAT))) {
            OBJ_DESTRUCT(stats);
            return rc;
        }
        OBJ_DESTRUCT(stats);

        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &proc_mem_info.share, 1, OPAL_INT64))) {
            return rc;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &proc_mem_info.minor_faults, 1, OPAL_INT64))) {
            return rc;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &proc_mem_info.major_faults, 1, OPAL_INT64))) {
            return rc;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &proc_mem_info.page_faults, 1, OPAL_INT64))) {
            return rc;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(dataptr, &proc_cpu_info.percent, 1, OPAL_DOUBLE))) {
            return rc;
        }
    }
    
    return ORCM_SUCCESS;
}
static void sigar_sample(orcm_sensor_sampler_t *sampler)
{
    opal_buffer_t data, *bptr;
    int rc;
    time_t now;
    double tdiff;
    char *ctmp;
    char time_str[40];
    char *timestamp_str;
    struct tm *sample_time;
    bool log_group=false;

    if (mca_sensor_sigar_component.test) {
        /* just send the test vector */
        bptr = &test_vector;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        return;
    }

    /* prep the buffer to collect the data */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    /* pack our name */
    ctmp = strdup("sigar");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &ctmp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(ctmp);
    /* include our node name */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* get the sample time */
    now = time(NULL);
    tdiff = difftime(now, last_sample);
    /* pass the time along as a simple string */
    sample_time = localtime(&now);
    if (NULL == sample_time) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    }
    strftime(time_str, sizeof(time_str), "%F %T%z", sample_time);
    asprintf(&timestamp_str, "%s", time_str);

    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &timestamp_str, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(timestamp_str);

    /* 1. Memory stats*/
    if(mca_sensor_sigar_component.mem) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_mem(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
    
    /* 2. Swap Memory stats */
    if(mca_sensor_sigar_component.swap) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_swap(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* 3. CPU stats*/
    if(mca_sensor_sigar_component.cpu) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_cpu(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* 4. load stats */
    if(mca_sensor_sigar_component.load) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_load(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* 5. disk stats */
    if(mca_sensor_sigar_component.disk) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_disk(&data, tdiff))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* 6. network stats */
    if(mca_sensor_sigar_component.network) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_network(&data, tdiff))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* 7. proc stats */
    if(mca_sensor_sigar_component.proc) {
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_global_procstat(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
        if(ORCM_ERR_SENSOR_READ_FAIL == (rc = sigar_collect_procstat(&data))) {
            ORTE_ERROR_LOG(rc);
        } else if (ORCM_SUCCESS != rc) {
            OBJ_DESTRUCT(&data);
            ORTE_ERROR_LOG(rc);
            return;
        }
    } else {
        log_group = false;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }

    /* No More process stats to pack - So pack the final marker*/
    log_group = false;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &log_group, 1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* xfer the data for transmission - need at least one prior sample before doing so */
    if (0 < last_sample) {
        bptr = &data;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
    }
    OBJ_DESTRUCT(&data);
    last_sample = now;
}

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void sigar_log(opal_buffer_t *sample)
{
    char *hostname;
    char *sampletime,global_ts[30];
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;
    uint64_t uint64;
    int64_t int64;
    float fval;
    bool log_group = false, data_avail = false;
    double sample_double;
    char processkey[MAX_PROC_NAME];
    opal_pstats_t *st;

    if (!log_enabled) {
        return;
    }


    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(3, orcm_sensor_base_framework.framework_output,
                        "%s Received log from host %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == hostname) ? "NULL" : hostname);

    /* prep the xfr storage */
    vals = OBJ_NEW(opal_list_t);

    /* sample time */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    if (NULL == sampletime) {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
        return;
    } else {
        strncpy(global_ts,sampletime,30);
        free(sampletime);
    }
    kv = OBJ_NEW(opal_value_t);
    kv->key = strdup("ctime");
    kv->type = OPAL_STRING;
    kv->data.string = strdup(global_ts);
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

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true;
        /* total memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("mem_total:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* total used memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("mem_used:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* actual used memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("mem_actual_used:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* actual free memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("mem_actual_free:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true;
        /* total swap memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_total:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* swap used */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_used:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* swap pages in */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_page_in:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* swap pages out */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("swap_page_out:kB");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true;
        /* cpu user */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cpu_user:%");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);

        /* cpu sys */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cpu_sys:%");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);

        /* cpu idle */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cpu_idle:%");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true;
        /* la0 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("load0");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);

        /* la5 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("load1");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);

        /* la15 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &fval, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("load2");
        kv->type = OPAL_FLOAT;
        kv->data.fval = fval;
        opal_list_append(vals, &kv->super);
    }
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true;  
        /* disk read ops rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("disk_ro_rate:ops/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* disk write ops rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("disk_wo_rate:ops/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* disk read bytes/sec */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("disk_rb_rate:bytes/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* disk write bytes/sec */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("disk_wb_rate:bytes/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);
    }
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        data_avail = true; 
        /* net recv packet rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("net_rp_rate:packets/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* net tx packet rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("net_wp_rate:packets/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* net recv bytes rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("net_rb_rate:bytes/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);

        /* net tx bytes rate */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint64, &n, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("net_wb_rate:bytes/sec");
        kv->type = OPAL_UINT64;
        kv->data.uint64 = uint64;
        opal_list_append(vals, &kv->super);
    }
    
    /* store it */
    if ((0 <= orcm_sensor_base.dbhandle) & (true == data_avail)) {
        orcm_db.store(orcm_sensor_base.dbhandle, "sigar", vals, mycleanup, NULL);
    } else {
        OPAL_LIST_RELEASE(vals);
    }

    /* Unpack the collected process stats and store them in the database */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(log_group) {
        vals = OBJ_NEW(opal_list_t);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(global_ts);
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

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("total_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("sleeping_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("running_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);
        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("zombie_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("stopped_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("idle_processes");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process threads */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("total_threads");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store(orcm_sensor_base.dbhandle, "procstat", vals, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(vals);
        }
    }
    /* Check if any process level stats are being sent */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    while(true == log_group) {
        vals = OBJ_NEW(opal_list_t);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(global_ts);
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

        /* process id */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &st, &n, OPAL_PSTAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
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
        if (NULL == kv->data.string) {
            return;
        }
        kv->data.string[0] = st->state[0];
        kv->data.string[1] = st->state[1];
        kv->data.string[2] = '\0';
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
        kv->key = strdup("vsize:Bytes");
        kv->type = OPAL_FLOAT;
        kv->data.fval = st->vsize;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("rss:Bytes");
        kv->type = OPAL_FLOAT;
        kv->data.fval = st->rss;
        opal_list_append(vals, &kv->super);

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("processor");
        kv->type = OPAL_INT16;
        kv->data.int16 = st->processor;
        opal_list_append(vals, &kv->super);

        /* process Shared Memory */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("shared memory");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process minor_faults */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("minor_faults");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process major_faults */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("major_faults");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process total_page_faults */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &int64, &n, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("page_faults");
        kv->type = OPAL_INT64;
        kv->data.int64 = int64;
        opal_list_append(vals, &kv->super);

        /* process cpu percent */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_double, &n, OPAL_DOUBLE))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("percent");
        kv->type = OPAL_DOUBLE;
        kv->data.dval = sample_double;
        opal_list_append(vals, &kv->super);

        snprintf(processkey,MAX_PROC_NAME, "procstat_%s",st->cmd);
       /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store(orcm_sensor_base.dbhandle, processkey, vals, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(vals);            
        }

        /* process cpu percent */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &log_group, &n, OPAL_BOOL))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        OBJ_DESTRUCT(st);
    } 
    if (NULL != hostname) {
        free(hostname);
    }
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

static void generate_test_vector(opal_buffer_t *v)
{
    char *ctmp, *date;
    uint64_t ui64;
    float ft;
    time_t now;

    ctmp = strdup("sigar");
    opal_dss.pack(v, &ctmp, 1, OPAL_STRING);
    free(ctmp);
    opal_dss.pack(v, &orte_process_info.nodename, 1, OPAL_STRING);
    /* get the time so it will be unique each time */
    now = time(NULL);
    /* pass the time along as a simple string */
    date = ctime(&now);
    if(NULL != date && 0 != strlen(date)) {
        /* strip the trailing newline */
        date[strlen(date)-1] = '\0';
    }
    opal_dss.pack(v, &date, 1, OPAL_STRING);
    /* mem_total */
    ui64 = 1;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* mem_used */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* mem_actual_used */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* mem_actual_free */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* swap total */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* swap used */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* swap page in */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* swap page out */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* cpu user */
    ft = 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* cpu sys */
    ft += 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* cpu idle */
    ft += 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* la */
    ft += 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* la5 */
    ft += 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* la15 */
    ft += 1.0;
    opal_dss.pack(v, &ft, 1, OPAL_FLOAT);
    /* reads */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* writes */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* read bytes */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* write bytes */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* rx packets */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* tx packets */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* rx bytes */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
    /* tx bytes */
    ui64++;
    opal_dss.pack(v, &ui64, 1, OPAL_UINT64);
}

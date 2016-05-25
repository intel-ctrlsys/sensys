/*
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal/dss/dss.h"

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <string.h>
#include <pthread.h>

#define HAVE_HWLOC_DIFF  // protect the hwloc diff.h file from ipmicmd.h conflict
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"
#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"
#include "orcm/mca/analytics/analytics.h"

#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "opal/runtime/opal_progress_threads.h"

#include "sensor_ipmi.h"
#include <../share/ipmiutil/isensor.h>
#include "sensor_ipmi_sel.h"

#include "ipmi_parser_interface.h"

#define ORCM_ON_NULL_CONTINUE(x) \
    if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);continue;}

typedef struct {
    opal_event_base_t *ev_base;
    bool ev_active;
    int sample_rate;
} orcm_sensor_ipmi_t;

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void ipmi_sample(orcm_sensor_sampler_t *sampler);
static void perthread_ipmi_sample(int fd, short args, void *cbdata);
void collect_ipmi_sample(orcm_sensor_sampler_t *sampler);
static void ipmi_log(opal_buffer_t *buf);
static void ipmi_inventory_collect(opal_buffer_t *inventory_snapshot);
static void ipmi_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
static void ipmi_set_sample_rate(int sample_rate);
static void ipmi_get_sample_rate(int *sample_rate);
bool does_sensor_group_match_sensor_name(char* sensor_group, char* sensor_name);
void orcm_sensor_ipmi_get_sel_events(ipmi_capsule_t* capsule);
void orcm_sensor_sel_ras_event_callback(const char* event, const char* hostname, void* user_object);
void orcm_sensor_sel_error_callback(int level, const char* msg);
int ipmi_enable_sampling(const char* sensor_specification);
int ipmi_disable_sampling(const char* sensor_specification);
int ipmi_reset_sampling(const char* sensor_specification);
void collect_ipmi_subsequent_data(orcm_sensor_sampler_t* sampler);
int collect_ipmi_subsequent_data_for_host(opal_buffer_t* sampler, orcm_sensor_hosts_t* host);
static void generate_test_vector(orcm_sensor_sampler_t* sampler);
static void generate_test_vector_inner(opal_buffer_t* buffer);
static void generate_test_vector_inv(opal_buffer_t *inventory_snapshot);
static int orcm_sensor_ipmi_get_sensor_inventory_list(opal_list_t *sensor_inventory);
static int update_host_info_from_config_file(orcm_sensor_hosts_t* host);
static int get_sensor_inventory_list_from_node(opal_list_t *inventory_list, ipmi_capsule_t *cap);

int first_sample = 0;

char **sensor_list_token; /* 2D array storing multiple sensor keywords for collecting metrics */

opal_list_t sensor_active_hosts; /* Hosts list for collecting metrics */
opal_list_t ipmi_inventory_hosts; /* Hosts list for storing inventory details */
static orcm_sensor_sampler_t *ipmi_sampler = NULL;
static orcm_sensor_ipmi_t orcm_sensor_ipmi;
static opal_list_t sensor_inventory;
static bool have_sensor_inventory = false;

static void ipmi_con(orcm_sensor_hosts_t *host)
{
}
static void ipmi_des(orcm_sensor_hosts_t *host)
{
}
OBJ_CLASS_INSTANCE(orcm_sensor_hosts_t,
                   opal_list_item_t,
                   ipmi_con, ipmi_des);

orcm_sensor_hosts_t *cur_host;

typedef struct {
    opal_list_item_t super;
    char *nodename;
    unsigned long hashId; /* A hash value summing up the inventory record for each node, for quick comparision */
    opal_list_t *records; /* An hwloc topology container followed by a list of inventory items */
} ipmi_inventory_t;

static void inv_con(ipmi_inventory_t *trk)
{
    trk->nodename = NULL;
    trk->hashId = 0;
    trk->records = OBJ_NEW(opal_list_t);
}
static void inv_des(ipmi_inventory_t *trk)
{
    ORCM_ON_NULL_RETURN(trk);
    SAFE_RELEASE(trk->records);
    SAFEFREE(trk->nodename);
}
OBJ_CLASS_INSTANCE(ipmi_inventory_t,
                   opal_list_item_t,
                   inv_con, inv_des);


/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_ipmi_module = {
    init,
    finalize,
    start,
    stop,
    ipmi_sample,
    ipmi_log,
    ipmi_inventory_collect,
    ipmi_inventory_log,
    ipmi_set_sample_rate,
    ipmi_get_sample_rate,
    ipmi_enable_sampling,
    ipmi_disable_sampling,
    ipmi_reset_sampling
};

/* local variables */
static opal_buffer_t test_vector;

char ipmi_inv_tv[10][2][30] = {
{"bmc_ver","9.9"},
{"ipmi_ver","8.8"},
{"bb_serial","TV_BbSer"},
{"bb_vendor","TV_BbVen"},
{"bb_manufactured_date","TV_MaufDat"},
{"sensor_ipmi_1","System Airflow"},
{"sensor_ipmi_2","LAN NIC Temp"},
{"sensor_ipmi_3","Processor 1 Fan"},
{"sensor_ipmi_4","Processor 2 Fan"},
{"sensor_ipmi_5","Exit Air Temp"}};

#define ON_NULL_GOTO(OBJ,LABEL) if(NULL==OBJ) { ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE); goto LABEL; }
#define ON_FAILURE_GOTO(RV,LABEL) if(ORCM_SUCCESS!=RV) { ORTE_ERROR_LOG(RV); goto LABEL; }
#define ORCM_RELEASE(x) if(NULL!=x){OBJ_RELEASE(x); x=NULL;}

static int init(void)
{
    int rc;
    ipmi_collector* bmc_list = NULL;
    int number_of_nodes = 0;
    disable_ipmi = 0;

    mca_sensor_ipmi_component.diagnostics = 0;
    mca_sensor_ipmi_component.runtime_metrics =
        orcm_sensor_base_runtime_metrics_create("ipmi", orcm_sensor_base.collect_metrics,
                                                mca_sensor_ipmi_component.collect_metrics);

    if(0 != geteuid()) {
        return ORCM_ERR_PERM;
    }

    OBJ_CONSTRUCT(&sensor_inventory, opal_list_t);
    OBJ_CONSTRUCT(&sensor_active_hosts, opal_list_t);
    OBJ_CONSTRUCT(&ipmi_inventory_hosts, opal_list_t);
    cur_host = OBJ_NEW(orcm_sensor_hosts_t);

    if (mca_sensor_ipmi_component.test) {
        /* generate test vector */
        OBJ_CONSTRUCT(&test_vector, opal_buffer_t);
        generate_test_vector_inner(&test_vector);
        return OPAL_SUCCESS;
    }

    if (!load_ipmi_config_file()) {
        opal_output(0, "Unable to load ipmi configuration");
        return ORCM_ERROR;
    }

    if (!get_bmcs_for_aggregator(orte_process_info.nodename, &bmc_list, &number_of_nodes)) {
        opal_output(0, "Unable to collect ipmi configuration");
        return ORCM_ERROR;
    }

    // Add the node to the slave list of the aggregator
    for (int i = 0; i < number_of_nodes; ++i) {
        if (ORCM_SUCCESS != orcm_sensor_ipmi_addhost(&bmc_list[i], &sensor_active_hosts))
        {
            opal_output(0,"Unable to add the new host! Try restarting ORCM");
            orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-addhost-fail",
                   true, orte_process_info.nodename, bmc_list[i].hostname);
        }
    }
    SAFEFREE(bmc_list);

    rc = orcm_sensor_ipmi_get_sensor_inventory_list(&sensor_inventory);
    if(rc != ORCM_SUCCESS) {
        opal_output(0, "Unable to collect the current sensor inventory");
        return ORCM_ERROR;
    }
    have_sensor_inventory = true;

    return OPAL_SUCCESS;
}

static void finalize(void)
{
    orcm_sensor_base_runtime_metrics_destroy(mca_sensor_ipmi_component.runtime_metrics);
    mca_sensor_ipmi_component.runtime_metrics = NULL;

    if(0 != geteuid()) {
        return;
    }

    OPAL_LIST_DESTRUCT(&sensor_active_hosts);
    OPAL_LIST_DESTRUCT(&ipmi_inventory_hosts);
    OPAL_LIST_DESTRUCT(&sensor_inventory);
    OBJ_RELEASE(cur_host);
}

/*Start monitoring of local processes */
static void start(orte_jobid_t jobid)
{
    /* Select sensor list if no sensors are specified by the user */
    if((NULL==mca_sensor_ipmi_component.sensor_list) & (NULL==mca_sensor_ipmi_component.sensor_group))
    {
        sensor_list_token = opal_argv_split("PS0 Power In,PS0 Temperature",',');
    } else {
        sensor_list_token = opal_argv_split(mca_sensor_ipmi_component.sensor_list,',');
    }
    for(int i =0; i <opal_argv_count(sensor_list_token);i++)
    {
        opal_output(0,"Sensor %d: %s",i,sensor_list_token[i]);
    }
    if(NULL!=mca_sensor_ipmi_component.sensor_group)
    {
        opal_output(0, "sensor group selected: %s", mca_sensor_ipmi_component.sensor_group);
    }

    /* start a separate ipmi progress thread for sampling */
    if (mca_sensor_ipmi_component.use_progress_thread) {
        if (!orcm_sensor_ipmi.ev_active) {
            orcm_sensor_ipmi.ev_active = true;
            if (NULL == (orcm_sensor_ipmi.ev_base = opal_progress_thread_init("ipmi"))) {
                orcm_sensor_ipmi.ev_active = false;
                return;
            }
        }

        /* setup ipmi sampler */
        ipmi_sampler = OBJ_NEW(orcm_sensor_sampler_t);

        /* check if ipmi sample rate is provided for this*/
        if (!mca_sensor_ipmi_component.sample_rate) {
            mca_sensor_ipmi_component.sample_rate = orcm_sensor_base.sample_rate;
        }
        ipmi_sampler->rate.tv_sec = mca_sensor_ipmi_component.sample_rate;
        ipmi_sampler->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(orcm_sensor_ipmi.ev_base, &ipmi_sampler->ev,
                               perthread_ipmi_sample, ipmi_sampler);
        opal_event_evtimer_add(&ipmi_sampler->ev, &ipmi_sampler->rate);
    }else{
	 mca_sensor_ipmi_component.sample_rate = orcm_sensor_base.sample_rate;

    }

    return;
}

static void stop(orte_jobid_t jobid)
{
    if (orcm_sensor_ipmi.ev_active) {
        orcm_sensor_ipmi.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_progress_thread_pause("ipmi");
        OBJ_RELEASE(ipmi_sampler);
    }
    return;
}

static void ipmi_sample(orcm_sensor_sampler_t *sampler)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi : ipmi_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (!mca_sensor_ipmi_component.use_progress_thread) {
       collect_ipmi_sample(sampler);
    }

}

static void ipmi_log_extract_create_string(opal_buffer_t *sample, char *dest_string, size_t dest_str_size)
{
    char *extract_item = NULL;
    int n = 1;
    int rc;

    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &extract_item, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    size_t max_len = MIN(strlen(extract_item), dest_str_size - 1);
    strncpy(dest_string, extract_item, max_len);
    dest_string[max_len] = '\0';
    SAFEFREE(extract_item);
}

static void ipmi_log_new_node(opal_buffer_t *sample)
{
    char nodename[64], hostip[16], bmcip[16], baseboard_manuf_date[11], baseboard_manufacturer[30], baseboard_name[16], baseboard_serial[16], baseboard_part[16];
    int rc;
    int32_t n;
    struct timeval sampletime;
    orcm_value_t *sensor_metric = NULL;
    orcm_analytics_value_t *analytics_vals = NULL;
    opal_list_t *key = NULL;
    opal_list_t *non_compute_data = NULL;

    /* sample time - 1b */
    n=1;
    rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);

    /* Unpack the node_name - 2 */
    ipmi_log_extract_create_string(sample, nodename, sizeof(nodename));
    opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
                        "IPMI_LOG -> Node %s not found; Logging credentials", nodename);
    /* Unpack the host_ip - 3a */
    ipmi_log_extract_create_string(sample, hostip, sizeof(hostip));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unpacked host_ip(3a): %s",hostip);

    /* Unpack the bmcip - 4a */
    ipmi_log_extract_create_string(sample, bmcip, sizeof(bmcip));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unpacked BMC_IP(4a): %s",bmcip);

    /* Unpack the Baseboard Manufacture Date - 5a */
    ipmi_log_extract_create_string(sample, baseboard_manuf_date, sizeof(baseboard_manuf_date));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unpacked Baseboard Manufacture Date(5a): %s", baseboard_manuf_date);

    /* Unpack the Baseboard Manufacturer Name - 6a */
    ipmi_log_extract_create_string(sample, baseboard_manufacturer, sizeof(baseboard_manufacturer));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unpacked Baseboard Manufacturer Name(6a): %s", baseboard_manufacturer);

    /* Unpack the Baseboard Product Name - 7a */
    ipmi_log_extract_create_string(sample, baseboard_name, sizeof(baseboard_name));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unpacked Baseboard Product Name(7a): %s", baseboard_name);

    /* Unpack the Baseboard Serial Number - 8a */
    ipmi_log_extract_create_string(sample, baseboard_serial, sizeof(baseboard_serial));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Unpacked Baseboard Serial Number(8a): %s", baseboard_serial);

    /* Unpack the Baseboard Part Number - 9a */
    ipmi_log_extract_create_string(sample, baseboard_part, sizeof(baseboard_part));
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Unpacked Baseboard Part Number(9a): %s", baseboard_part);

    key = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_RETURN(key);

    non_compute_data = OBJ_NEW(opal_list_t);
    ORCM_ON_NULL_GOTO(non_compute_data,cleanup);

    sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("hostname", nodename, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "UnPacked NodeName: %s", nodename);

    sensor_metric = orcm_util_load_orcm_value("data_group", "ipmi", OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(key, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("BBmanuf_date", baseboard_manuf_date, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("BBmanuf", baseboard_manufacturer, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("BBname", baseboard_name, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("BBserial", baseboard_serial, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    sensor_metric = orcm_util_load_orcm_value("BBpart", baseboard_part, OPAL_STRING, NULL);
    ORCM_ON_NULL_GOTO(sensor_metric, cleanup);
    opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

    analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, NULL);
    ORCM_ON_NULL_GOTO(analytics_vals, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->key, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->non_compute_data, cleanup);
    ORCM_ON_NULL_GOTO(analytics_vals->compute_data, cleanup);

    orcm_analytics.send_data(analytics_vals);
    if ( NULL != analytics_vals) {
        OBJ_RELEASE(analytics_vals);
    }

 cleanup:
    if ( NULL != key) {
        OBJ_RELEASE(key);
    }
    if ( NULL != non_compute_data) {
        OBJ_RELEASE(non_compute_data);
    }

    return;

}

static void ipmi_log_existing_multiple_hosts(opal_buffer_t *sample, int host_count)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Total Samples to be unpacked: %d", host_count);
    /* START UNPACKING THE DATA and Store it in a opal_list_t item. */
    for(int count = 0; count < host_count; count++)
    {
        char *hostname = NULL;
        int rc;
        int32_t n;
        struct timeval sampletime;
        orcm_value_t *sensor_metric = NULL;
        opal_list_t *key = NULL;
        opal_list_t *non_compute_data = NULL;
        opal_list_t* data_list = NULL;
        bool completed = false;

        key = OBJ_NEW(opal_list_t);
        ON_NULL_GOTO(key, cleanup);

        non_compute_data = OBJ_NEW(opal_list_t);
        ON_NULL_GOTO(non_compute_data, cleanup);

        /* sample time - 2 */
        n=1;
        rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL);
        ON_FAILURE_GOTO(rc, cleanup);

        sensor_metric = orcm_util_load_orcm_value("ctime", &sampletime, OPAL_TIMEVAL, NULL);
        ON_NULL_GOTO(sensor_metric, cleanup);
        opal_list_append(non_compute_data, (opal_list_item_t *)sensor_metric);

        /* Unpack the node_name - 3 */
        n=1;
        rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING);
        ON_FAILURE_GOTO(rc, cleanup);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked NodeName: %s", hostname);

        sensor_metric = orcm_util_load_orcm_value("hostname", hostname, OPAL_STRING, NULL);
        ON_NULL_GOTO(sensor_metric, cleanup);
        opal_list_append(key, (opal_list_item_t *)sensor_metric);

        /* Pack sensor name */
        sensor_metric = orcm_util_load_orcm_value("data_group", "ipmi", OPAL_STRING, NULL);
        ON_NULL_GOTO(sensor_metric, cleanup);
        opal_list_append(key, (opal_list_item_t *)sensor_metric);

        orcm_sensor_unpack_orcm_value_list(sample, &data_list);
        ORCM_ON_NULL_GOTO(data_list, cleanup);
        orcm_analytics_value_t *analytics_vals = NULL;
        OPAL_LIST_FOREACH(sensor_metric, data_list, orcm_value_t) {
            if (NULL != sensor_metric->units && 0 == strlen(sensor_metric->units)) {
                SAFEFREE(sensor_metric->units);
            }
        }

        analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute_data, data_list);
        ON_NULL_GOTO(analytics_vals, cleanup);
        orcm_analytics.send_data(analytics_vals);
        ORCM_RELEASE(analytics_vals);

        completed = true;

    cleanup:
        if (!completed) {
            opal_output_verbose(0, orcm_sensor_base_framework.framework_output,
                                "Possible Error: Failed to collect ipmi data from node : '%s'",
                                hostname);
        }
        ORCM_RELEASE(data_list);
        SAFEFREE(hostname);
        ORCM_RELEASE(key);
        ORCM_RELEASE(non_compute_data);
    }
}

static void ipmi_log(opal_buffer_t *sample)
{
    int host_count;
    int n;
    int rc;

    if(disable_ipmi == 1)
        return;

    /* Unpack the host_count identifer */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &host_count, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if (host_count==0) {
    /*New Node is getting added */
        ipmi_log_new_node(sample);
        return;
    }
    else {
        ipmi_log_existing_multiple_hosts(sample, host_count);
    }

}

static void ipmi_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int rc;
    unsigned int tot_items = 0;
    const char *comp = "ipmi";
    opal_value_t* inv_item = NULL;
    struct timeval time_stamp;

    if (mca_sensor_ipmi_component.test) {
        /* generate test vector */
        generate_test_vector_inv(inventory_snapshot);
        return;
    }

    ORCM_ON_NULL_GOTO(cur_host, ipmi_inventory_collect_error);

    tot_items = ((unsigned int)opal_list_get_size(&sensor_inventory) / 2) + 5;

    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    gettimeofday(&time_stamp, NULL);
    rc = opal_dss.pack(inventory_snapshot, &time_stamp, 1, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = "bmc_ver";
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = cur_host->capsule.prop.bmc_rev;
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = "ipmi_ver";
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = cur_host->capsule.prop.ipmi_ver;
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = "bb_serial";
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = cur_host->capsule.prop.baseboard_serial;
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = "bb_vendor";
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = cur_host->capsule.prop.baseboard_manufacturer;
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = "bb_manufactured_date";
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    comp = cur_host->capsule.prop.baseboard_manuf_date;
    rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, ipmi_inventory_collect_error);

    OPAL_LIST_FOREACH(inv_item, &sensor_inventory, opal_value_t) {
        comp = inv_item->data.string;
        rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
        ORCM_ON_FAILURE_RETURN(rc);
    }

    return;

ipmi_inventory_collect_error:
    opal_output_verbose(0, orcm_sensor_base_framework.framework_output,
                        "Possible Error: Failed to collect ipmi inventory'");

    return;
}

int orcm_sensor_get_fru_inv(orcm_sensor_hosts_t *host)
{
    int ret = 0;
    unsigned char idata[4], rdata[MAX_FRU_DEVICES][MAX_IPMI_RESPONSE];
    unsigned char ccode;
    int rlen = MAX_IPMI_RESPONSE;
    int id;
    int max_id = 0;;
    unsigned char hex_val=0;
    long int fru_area;
    long int max_fru_area = 0;
    char *error_string;
    ipmi_capsule_t *cap = NULL;
    char addr[16];

    ORCM_ON_NULL_RETURN_ERROR(host, ORCM_ERROR);
    cap = &host->capsule;

    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);

    memset(idata,0x00,sizeof(idata));
    for (id = 0; id < MAX_FRU_DEVICES; id++) {
        memset(rdata[id], 0x00, MAX_IPMI_RESPONSE);
        *idata = hex_val;
        ret = ipmi_cmd(GET_FRU_INV_AREA, idata, 1, rdata[id], &rlen, &ccode, 0);
        if(ret)
        {
            error_string = decode_rv(ret);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "ipmi_cmd_mc for get_fru_inv RETURN CODE : %s \n", error_string);
        }
        ipmi_close();
        hex_val++;
    }
    /*
    Now that we have the size of each fru device, we want to find the one
    with the largest size, as that is the one we'll want to read from.
    */
    for (id = 0; id < MAX_FRU_DEVICES; id++) {
        /* Convert the hex value in rdata to decimal so we can compare it.*/
        fru_area = rdata[id][0] | (rdata[id][1] << 8) | (rdata[id][2] << 16) | (rdata[id][3] << 24);

        /* If the newest area is the larget, set the max size to that and
         * mark the max id to be that id. */
        if (fru_area > max_fru_area) {
            max_fru_area = fru_area;
            max_id = id;
        }
    }
    return orcm_sensor_get_fru_data(max_id, max_fru_area, host);
}

int orcm_sensor_ipmi_get_manuf_date (unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    unsigned int manuf_time[3]; /*holds the manufactured time (in minutes) from 0:00 1/1/96*/
    unsigned long int manuf_minutes; /*holds the manufactured time (in minutes) from 0:00 1/1/96*/
    unsigned long int manuf_seconds; /*holds the above time (in seconds)*/
    time_t raw_seconds;
    struct tm *time_info;
    char manuf_date[11]; /*A mm/dd/yyyy or dd/mm/yyyy formatted date 10 + 1 for null byte*/

    /*IPMI time is stored in minutes from 0:00 1/1/1996*/
    manuf_time[0] = rdata[fru_offset + 3]; /*LSByte of the time*/
    manuf_time[1] = rdata[fru_offset + 4]; /*MiddleByte of the time*/
    manuf_time[2] = rdata[fru_offset + 5]; /*MSByte of the time*/

    /*Convert to 1 value*/
    manuf_minutes = manuf_time[0] + (manuf_time[1] << 8) + (manuf_time[2] << 16);
    manuf_seconds = manuf_minutes * 60;

    /*Time from epoch = time from ipmi start + difference from epoch to ipmi start*/
    raw_seconds = manuf_seconds + EPOCH_IPMI_DIFF_TIME;
    time_info = localtime(&raw_seconds);
    ORCM_ON_NULL_RETURN_ERROR(time_info, ORCM_ERR_OUT_OF_RESOURCE);
    strftime(manuf_date,10,"%x",time_info);

    strncpy(host->capsule.prop.baseboard_manuf_date, manuf_date, sizeof(host->capsule.prop.baseboard_manuf_date)-1);
    host->capsule.prop.baseboard_manuf_date[sizeof(host->capsule.prop.baseboard_manuf_date)-1] = '\0';

    return 0;
}

int orcm_sensor_ipmi_get_manuf_name (unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    int i=0;
    unsigned char board_manuf_length; /*holds the length (in bytes) of board manuf name*/
    char *board_manuf = NULL; /*hold board manufacturer*/

    board_manuf_length = rdata[fru_offset + BOARD_INFO_DATA_START] & 0x3f;
    board_manuf = (char*) malloc (board_manuf_length + 1); /* + 1 for the Null Character */
    ORCM_ON_NULL_RETURN_ERROR(board_manuf, ORCM_ERR_OUT_OF_RESOURCE);

    for(i = 0; i < board_manuf_length; i++){
        board_manuf[i] = rdata[fru_offset + BOARD_INFO_DATA_START + 1 + i];
    }

    board_manuf[i] = '\0';
    strncpy(host->capsule.prop.baseboard_manufacturer, board_manuf, sizeof(host->capsule.prop.baseboard_manufacturer)-1);
    free(board_manuf);
    host->capsule.prop.baseboard_manufacturer[sizeof(host->capsule.prop.baseboard_manufacturer)-1] = '\0';

    return board_manuf_length;
}

int orcm_sensor_ipmi_get_product_name(unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    int i=0;
    unsigned char board_product_length; /*holds the length (in bytes) of board product name*/
    char *board_product_name; /*holds board product name*/

    board_product_length = rdata[fru_offset + BOARD_INFO_DATA_START] & 0x3f;
    board_product_name = (char*) malloc (board_product_length + 1); /* + 1 for the Null Character */
    ORCM_ON_NULL_RETURN_ERROR(board_product_name, ORCM_ERR_OUT_OF_RESOURCE);

    for(i = 0; i < board_product_length; i++) {
        board_product_name[i] = rdata[fru_offset + BOARD_INFO_DATA_START + 1 + i];
    }

    board_product_name[i] = '\0';
    strncpy(host->capsule.prop.baseboard_name, board_product_name, sizeof(host->capsule.prop.baseboard_serial)-1);
    host->capsule.prop.baseboard_name[sizeof(host->capsule.prop.baseboard_name)-1] = '\0';
    free(board_product_name);

    return board_product_length;
}

int orcm_sensor_ipmi_get_serial_number(unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    int i=0;
    unsigned char board_serial_length; /*holds length (in bytes) of board serial number*/
    char *board_serial_num; /*will hold board serial number*/

    board_serial_length = rdata[fru_offset + BOARD_INFO_DATA_START] & 0x3f;
    board_serial_num = (char*) malloc (board_serial_length + 1); /* + 1 for the Null Character */
    ORCM_ON_NULL_RETURN_ERROR(board_serial_num, ORCM_ERR_OUT_OF_RESOURCE);

    for(i = 0; i < board_serial_length; i++) {
        board_serial_num[i] = rdata[fru_offset + BOARD_INFO_DATA_START + 1 + i];
    }

    board_serial_num[i] = '\0';
    strncpy(host->capsule.prop.baseboard_serial, board_serial_num, sizeof(host->capsule.prop.baseboard_serial)-1);
    host->capsule.prop.baseboard_serial[sizeof(host->capsule.prop.baseboard_serial)-1] = '\0';
    free(board_serial_num);

    return board_serial_length;
}

int orcm_sensor_ipmi_get_board_part(unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    int i=0;
    unsigned char board_part_length; /*holds length (in bytes) of the board part number*/
    char *board_part_num; /*will hold board part number*/

    board_part_length = rdata[fru_offset + BOARD_INFO_DATA_START] & 0x3f;
    board_part_num = (char*) malloc (board_part_length + 1); /* + 1 for the Null Character */
    ORCM_ON_NULL_RETURN_ERROR(board_part_num, ORCM_ERR_OUT_OF_RESOURCE);

    for (i = 0; i < board_part_length; i++) {
        board_part_num[i] = rdata[fru_offset + BOARD_INFO_DATA_START + 1 + i];
    }

    board_part_num[i] = '\0';
    strncpy(host->capsule.prop.baseboard_part, board_part_num, sizeof(host->capsule.prop.baseboard_part)-1);
    host->capsule.prop.baseboard_part[sizeof(host->capsule.prop.baseboard_part)-1] = '\0';
    free(board_part_num);

    return board_part_length;
}

int orcm_sensor_get_fru_data(int id, long int fru_area, orcm_sensor_hosts_t *host)
{
    int ret;
    int i, ffail_count=0;
    int rlen = MAX_IPMI_RESPONSE;
    unsigned char idata[4];
    unsigned char tempdata[17];
    unsigned char *rdata;
    unsigned char ccode;
    int rdata_offset = 0;
    unsigned char fru_offset;

    rdata = (unsigned char*) malloc(fru_area);
    ORCM_ON_NULL_RETURN_ERROR(rdata, ORCM_ERROR);

    memset(rdata,0x00,sizeof(fru_area));
    memset(idata,0x00,sizeof(idata));

    idata[0] = id;   /*id of the fru device to read from*/
    idata[1] = 0x00; /*LSByte of the offset, start at 0*/
    idata[2] = 0x00; /*MSbyte of the offset, start at 0*/
    idata[3] = 0x10; /*reading 16 bytes at a time*/
    ret = 0;
    for (i = 0; i < (fru_area/16); i++) {
        memset(tempdata, 0x00, sizeof(tempdata));
        ret = ipmi_cmd(READ_FRU_DATA, idata, 4, tempdata, &rlen, &ccode, 0);
        if (ret) {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "FRU Read Number %d retrying in block %d\n", id, i);
            ipmi_close();
            if (ffail_count > 15)
            {
                orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-fru-read-fail",
                               true, orte_process_info.nodename);
                free(rdata);
                return ORCM_ERROR;
            } else {
                i--;
                ffail_count++;
                continue;
            }
        }
        ipmi_close();

        /* Copy what was read in to the next 16 byte section of rdata
         * and then increment the offset by another 16 for the next read */
        memcpy(rdata + rdata_offset, &tempdata[1], 16);
        rdata_offset += 16;

        /*We need to increment the MSByte instead of the LSByte*/
        if (idata[1] == 240) {
            idata[1] = 0x00;
            idata[2]++;
        } else {
            idata[1] += 0x10;
        }
    }

    /*
        Source: Platform Management Fru Document (Rev 1.2)
        Fru data is stored in the following fashion:
        1 Byte - N number of bytes to follow that holds some information
        N Bytes - The information we are after

        So the location of information within rdata is always relative to the
        location of the information that came before it.

        To get to the size of the information to follow, skip past all the
        information you've already read. To the read that information, skip
        past all the information you've already read + 1, then read that number
        of bytes.
    */

    /* Board Info */
    fru_offset = rdata[3] * 8; /*Board starting offset is stored in 3, multiples of 8 bytes*/

    if(0 <= (ret= orcm_sensor_ipmi_get_manuf_date(fru_offset, rdata, host))) {
    } else {
        free(rdata);
        return ORCM_ERROR;
    }

    if(0 <= (ret = orcm_sensor_ipmi_get_manuf_name(fru_offset, rdata, host))) {
        fru_offset+=(ret+1); /*Increment offset to ignore terminating null character */
    } else {
        free(rdata);
        return ORCM_ERROR;
    }

    if(0 <= (ret = orcm_sensor_ipmi_get_product_name(fru_offset, rdata, host))) {
        fru_offset+=(ret+1);
    } else {
        free(rdata);
        return ORCM_ERROR;
    }

    if(0 <= (ret = orcm_sensor_ipmi_get_serial_number(fru_offset, rdata, host))) {
        fru_offset+=(ret+1);
    } else {
        free(rdata);
        return ORCM_ERROR;
    }

    if(0 <= (ret = orcm_sensor_ipmi_get_board_part(fru_offset, rdata, host))) {
        fru_offset+=(ret+1);
    } else {
        free(rdata);
        return ORCM_ERROR;
    }

    free(rdata);
    return ORCM_SUCCESS;
}

/* int orcm_sensor_ipmi_found (char* nodename, opal_list_t * host_list)
 * Return ORCM_SUCCESS if nodename matches an existing node in the host_list
 * Return ORCM_ERR_NOT_FOUND if nodename doesn't match
 */
int orcm_sensor_ipmi_found(char *nodename, opal_list_t *host_list)
{
    orcm_sensor_hosts_t *host, *nxt;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node: %s", nodename);
    OPAL_LIST_FOREACH_SAFE(host, nxt, host_list, orcm_sensor_hosts_t) {
        if(!strcmp(nodename,host->capsule.node.name))
        {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Found Node: %s", nodename);
            return ORCM_SUCCESS;
        }
    }
    return ORCM_ERR_NOT_FOUND;
}

int orcm_sensor_ipmi_addhost(ipmi_collector* ic, opal_list_t *host_list)
{
    orcm_sensor_hosts_t *newhost;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Adding New Node: %s, with BMC IP: %s", ic->hostname, ic->bmc_address);

    newhost = OBJ_NEW(orcm_sensor_hosts_t);
    strncpy(newhost->capsule.node.name,ic->hostname,sizeof(newhost->capsule.node.name)-1);
    newhost->capsule.node.name[sizeof(newhost->capsule.node.name)-1] = '\0';
    strncpy(newhost->capsule.node.host_ip,ic->hostname,sizeof(newhost->capsule.node.host_ip)-1);
    newhost->capsule.node.host_ip[sizeof(newhost->capsule.node.host_ip)-1] = '\0';
    strncpy(newhost->capsule.node.bmc_ip,ic->bmc_address,sizeof(newhost->capsule.node.bmc_ip)-1);
    newhost->capsule.node.bmc_ip[sizeof(newhost->capsule.node.bmc_ip)-1] = '\0';
    strncpy(newhost->capsule.node.user,ic->user,sizeof(newhost->capsule.node.user)-1);
    newhost->capsule.node.user[sizeof(newhost->capsule.node.user)-1] = '\0';
    strncpy(newhost->capsule.node.pasw,ic->pass,sizeof(newhost->capsule.node.pasw)-1);
    newhost->capsule.node.pasw[sizeof(newhost->capsule.node.pasw)-1] = '\0';

    newhost->capsule.node.auth = ic->auth_method;
    newhost->capsule.node.priv = ic->priv_level;
    newhost->capsule.node.ciph = 3; /* Cipher suite No. 3 */

    /* Add to Host list */
    opal_list_append(host_list, &newhost->super);

    return ORCM_SUCCESS;
}

int orcm_sensor_ipmi_label_found(char *sensor_label)
{
    int i;
    for (i = 0; i< opal_argv_count(sensor_list_token);i++)
    {
        if(!strncmp(sensor_list_token[i],sensor_label,strlen(sensor_list_token[i])))
        {
            return 1;
        }
    }
    return 0;
}

static ipmi_inventory_t* found_inventory_host(char * nodename)
{
    ipmi_inventory_t *host;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node in inventory inventory: %s", nodename);
    OPAL_LIST_FOREACH(host, &ipmi_inventory_hosts, ipmi_inventory_t) {
        if(!strcmp(nodename,host->nodename))
        {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Found Node: %s", nodename);
            return host;
        }
    }
    return NULL;
}

static bool compare_ipmi_record (ipmi_inventory_t* newhost , ipmi_inventory_t* oldhost)
{
    orcm_value_t *newitem, *olditem;
    unsigned int count = 0, record_size = 0;
    /* @VINFIX: Need to come up with a clever way to implement comparision of different
     * ipmi inventory records
     */
    if((record_size = opal_list_get_size(newhost->records)) != opal_list_get_size(oldhost->records))
    {
        opal_output(0,"IPMI Inventory compare failed: Unequal item count;");
        return false;
    }
    newitem = (orcm_value_t*)opal_list_get_first(newhost->records);
    olditem = (orcm_value_t*)opal_list_get_first(oldhost->records);

    for(count = 0; count < record_size ; count++) {
        ORCM_ON_NULL_GOTO(newitem, compare_ipmi_record_error);
        ORCM_ON_NULL_GOTO(olditem, compare_ipmi_record_error);

        if(newitem->value.type != olditem->value.type) {
            opal_output(0,"IPMI inventory records mismatch: value.type mismatch");
            return false;
        } else {
            if(OPAL_STRING == newitem->value.type) {
                if(strcmp(newitem->value.key, olditem->value.key)) {
                    opal_output(0,"IPMI inventory records mismatch: value.key mismatch");
                    return false;
                } else if (strcmp(newitem->value.data.string, olditem->value.data.string)) {
                    opal_output(0,"IPMI inventory records mismatch: value.data.string mismatch");
                    return false;
                }
            } else if ((OPAL_FLOAT == newitem->value.type)){
                if(newitem->value.data.fval != olditem->value.data.fval) {
                    opal_output(0,"IPMI inventory records mismatch: value.data.fval mismatch");
                    return false;
                }
            } else {
                opal_output(0,"Invalid data stored as inventory with invalid data type %d", newitem->value.type);
                return false;
            }
        }
        newitem = (orcm_value_t*)opal_list_get_next(newitem);
        olditem = (orcm_value_t*)opal_list_get_next(olditem);
    }

    return true;

compare_ipmi_record_error:
    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "Unable to retrieve host record, will mark mismatch");
    return false;
}

static void ipmi_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    char *inv = NULL, *inv_val = NULL;
    unsigned int tot_items;
    int rc, n;
    ipmi_inventory_t *newhost = NULL, *oldhost = NULL;
    orcm_value_t *mkv = NULL, *mkv_copy = NULL;
    opal_value_t *kv = NULL;
    orcm_value_t *time_stamp = NULL;
    struct timeval current_time;

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &current_time, &n, OPAL_TIMEVAL);
    ORCM_ON_FAILURE_RETURN(rc);

    n=1;
    rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT);
    ORCM_ON_FAILURE_RETURN(rc);

    newhost = OBJ_NEW(ipmi_inventory_t);
    ORCM_ON_NULL_RETURN(newhost);

    while(tot_items > 0) {
        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc, cleanup);

        n=1;
        rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING);
        ORCM_ON_FAILURE_GOTO(rc, cleanup);

        time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
        ORCM_ON_NULL_RETURN(time_stamp);

        mkv = OBJ_NEW(orcm_value_t);
        mkv->value.key = inv;

        if (!strncmp(inv,"hostname",sizeof("hostname"))) {
            newhost->nodename = strdup(inv_val);
            ORCM_ON_NULL_CONTINUE(newhost->nodename);
            SAFEFREE(inv_val);
            tot_items--;

            if(NULL != (oldhost = found_inventory_host(newhost->nodename))) {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "Host '%s' Found, comparing values",hostname);

                if(false == compare_ipmi_record(newhost, oldhost)) {
                    opal_output(0,"IPMI Compare failed; Notify User; Update List; Update Database");
                    ORCM_RELEASE(oldhost->records);
                    oldhost->records=OBJ_NEW(opal_list_t);
                    OPAL_LIST_FOREACH(mkv, newhost->records, orcm_value_t) {
                        mkv_copy = OBJ_NEW(orcm_value_t);
                        mkv_copy->value.key = strdup(mkv->value.key);

                        if(!strncmp(mkv->value.key,"bmc_ver",sizeof("bmc_ver")) || !strncmp(mkv->value.key,"ipmi_ver",sizeof("ipmi_ver")))
                        {
                            mkv_copy->value.type = OPAL_FLOAT;
                            mkv_copy->value.data.fval = mkv->value.data.fval;
                        } else {
                            mkv_copy->value.type = OPAL_STRING;
                            mkv_copy->value.data.string = strdup(mkv->value.data.string);
                            ORCM_ON_NULL_CONTINUE(mkv_copy->value.data.string);
                        }
                        opal_list_append(oldhost->records,(opal_list_item_t *)mkv_copy);
                        mkv_copy = NULL;
                    }
                    ORCM_RELEASE(newhost);

                    kv = orcm_util_load_opal_value("hostname", oldhost->nodename, OPAL_STRING);
                    ORCM_ON_NULL_GOTO(kv, cleanup);
                    opal_list_prepend(oldhost->records, &kv->super);
                    kv = NULL;
                    opal_list_prepend(oldhost->records, (opal_list_item_t*)time_stamp);

                    /* Send the collected inventory details to the database for storage */
                    if (0 <= orcm_sensor_base.dbhandle) {
                        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA , oldhost->records, NULL, NULL, NULL);
                    }
                } else {
                    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                        "ipmi compare passed");
                }
            } else {
                newhost->nodename = strdup(hostname);
                kv = orcm_util_load_opal_value("hostname", newhost->nodename, OPAL_STRING);
                ORCM_ON_NULL_GOTO(kv, cleanup);

                opal_list_prepend(newhost->records, &kv->super);
                kv = NULL;

                opal_list_prepend(newhost->records, (opal_list_item_t*)time_stamp);

                /* Append the new node to the existing host list */
                opal_list_append(&ipmi_inventory_hosts, &newhost->super);

                /* Send the collected inventory details to the database for storage */
                if (0 <= orcm_sensor_base.dbhandle) {
                    orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA , newhost->records, NULL, NULL, NULL);
                }
                newhost = NULL; /* In list so don't release */
                newhost = OBJ_NEW(ipmi_inventory_t);
                ORCM_ON_NULL_RETURN(newhost);
            }

        } else {
           if(!strncmp(inv,"bmc_ver",sizeof("bmc_ver")) || !strncmp(inv,"ipmi_ver",sizeof("ipmi_ver"))) {
                mkv->value.type = OPAL_FLOAT;
                mkv->value.data.fval = strtof(inv_val,NULL);
                SAFEFREE(inv_val);
                inv_val = NULL;
            } else {
                mkv->value.type = OPAL_STRING;
                mkv->value.data.string = inv_val;
                inv_val = NULL;
            }

            inv = NULL;
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            mkv = NULL;
            tot_items--;
        }
    }

cleanup:
    SAFEFREE(inv_val);
}

static void ipmi_set_sample_rate(int sample_rate)
{
    /* set the ipmi sample rate if seperate thread is enabled */
    if (mca_sensor_ipmi_component.use_progress_thread) {
        mca_sensor_ipmi_component.sample_rate = sample_rate;
    }
    return;
}

static void ipmi_get_sample_rate(int *sample_rate)
{
    if (NULL != sample_rate) {
    /* check if ipmi sample rate is provided for this*/
            *sample_rate = mca_sensor_ipmi_component.sample_rate;
    }
    return;
}

static void perthread_ipmi_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi : perthread_ipmi_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* this has fired in the sampler thread, so we are okay to
     * just go ahead and sample since we do NOT allow both the
     * base thread and the component thread to both be actively
     * calling this component */
    collect_ipmi_sample(sampler);
    /* we now need to push the results into the base event thread
     * so it can add the data to the base bucket */
    ORCM_SENSOR_XFER(&sampler->bucket);
    /* clear the bucket */
    OBJ_DESTRUCT(&sampler->bucket);
    OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    /* check if ipmi sample rate is provided for this*/
    if (mca_sensor_ipmi_component.sample_rate != sampler->rate.tv_sec) {
        sampler->rate.tv_sec = mca_sensor_ipmi_component.sample_rate;
    }
    /* set ourselves to sample again */
    opal_event_evtimer_add(&sampler->ev, &sampler->rate);
}

static int update_host_info_from_config_file(orcm_sensor_hosts_t* host) {
    ipmi_collector ic;

    if (!get_bmc_info(host->capsule.node.name, &ic)) {
        opal_output(0, "Unable to retrieve configuration for node: %s", host->capsule.node.name);
        return ORCM_ERROR;
    }

    host->capsule.node.auth = ic.auth_method;
    host->capsule.node.priv = ic.priv_level;
    host->capsule.node.ciph = 3; /* Cipher suite No. 3 */

    strncpy(host->capsule.node.user, ic.user, sizeof(host->capsule.node.user)-1);
    host->capsule.node.user[sizeof(host->capsule.node.user)-1] = '\0';

    strncpy(host->capsule.node.pasw, ic.pass, sizeof(host->capsule.node.pasw)-1);
    host->capsule.node.pasw[sizeof(host->capsule.node.pasw)-1] = '\0';

    strncpy(host->capsule.node.bmc_ip, ic.bmc_address, sizeof(host->capsule.node.bmc_ip)-1);
    host->capsule.node.bmc_ip[sizeof(host->capsule.node.bmc_ip)-1] = '\0';

    return ORCM_SUCCESS;
}

int collect_ipmi_subsequent_data_for_host(opal_buffer_t* data, orcm_sensor_hosts_t* host)
{
    int rc;
    char* sample_str = NULL;
    struct timeval current_time;
    opal_list_t* data_list = NULL;
    orcm_value_t* item = NULL;
    void* metrics_obj = mca_sensor_ipmi_component.runtime_metrics;

    if (ORCM_SUCCESS != update_host_info_from_config_file(host) ) {
        opal_output(0, "Error reading configuration file for IPMI.");
        rc = ORCM_ERROR;
        ON_FAILURE_GOTO(rc, cleanup);
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Scanning metrics from node: %s",host->capsule.node.name);
    /* Clear all memory for the ipmi_capsule */
    memset(&(host->capsule.prop), '\0', sizeof(host->capsule.prop));

    /* Running a sample for a Node */
    orcm_sensor_ipmi_get_device_id(&host->capsule);
    orcm_sensor_ipmi_get_power_states(&host->capsule);
    orcm_sensor_ipmi_get_sensor_reading(&host->capsule);

    gettimeofday(&current_time, NULL);

    /* Pack ctime */
    rc = opal_dss.pack(data, &current_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_GOTO(rc, cleanup);

    /* Pack hostname */
    sample_str = (char*)&host->capsule.node.name;
    rc = opal_dss.pack(data, &sample_str, 1, OPAL_STRING);
    ON_FAILURE_GOTO(rc, cleanup);

    data_list = OBJ_NEW(opal_list_t);
    ON_NULL_GOTO(data_list, cleanup);

    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "bmcfwrev")) {
        sample_str = (char *)&(host->capsule.prop.bmc_rev[0]);
        item = orcm_util_load_orcm_value("bmcfwrev", sample_str, OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(data_list, (opal_list_item_t*)item);
    }

    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "ipmiver")) {
        sample_str = (char *)&(host->capsule.prop.ipmi_ver[0]);
        item = orcm_util_load_orcm_value("ipmiver", sample_str, OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(data_list, (opal_list_item_t*)item);
    }

    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "manufacturer_id")) {
        sample_str = (char *)&(host->capsule.prop.man_id[0]);
        item = orcm_util_load_orcm_value("manufacturer_id", sample_str, OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(data_list, (opal_list_item_t*)item);
    }

    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "sys_power_state")) {
        sample_str = (char *)&(host->capsule.prop.sys_power_state[0]);
        item = orcm_util_load_orcm_value("sys_power_state", sample_str, OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(data_list, (opal_list_item_t*)item);
    }

    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "dev_power_state")) {
        sample_str = (char *)&(host->capsule.prop.dev_power_state[0]);
        item = orcm_util_load_orcm_value("dev_power_state", sample_str, OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(data_list, (opal_list_item_t*)item);
    }

    /*  Pack the non-string metrics and their units */
    for(int count_metrics=0;count_metrics<host->capsule.prop.total_metrics;count_metrics++) {
        char* units = (char *)&host->capsule.prop.collection_metrics_units[count_metrics];
        sample_str = (char *)&host->capsule.prop.metric_label[count_metrics];
        if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, sample_str)) {
            units = (char *)&host->capsule.prop.collection_metrics_units[count_metrics];
            item = orcm_util_load_orcm_value(sample_str, &host->capsule.prop.collection_metrics[count_metrics], OPAL_FLOAT, units);
            ON_NULL_GOTO(item, cleanup);
            opal_list_append(data_list, (opal_list_item_t*)item);
        }
    }

    /* collect SEL events */
    if(orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, "sel_event_record")) {
        host->capsule.prop.collected_sel_records = data_list;
        orcm_sensor_ipmi_get_sel_events(&host->capsule);
        host->capsule.prop.collected_sel_records = NULL;
    }

    /* Pack all data at once */
    rc = orcm_sensor_pack_orcm_value_list(data, data_list);
    ON_FAILURE_GOTO(rc, cleanup);

cleanup:
    SAFE_RELEASE(data_list);
    return rc;
}

void collect_ipmi_subsequent_data(orcm_sensor_sampler_t* sampler)
{
    int rc;
    opal_buffer_t data;
    opal_buffer_t* bptr = NULL;
    size_t host_count = 0;
    char* ipmi = NULL;
    orcm_sensor_hosts_t* host;
    orcm_sensor_hosts_t* nxt;
    void* metrics_obj = mca_sensor_ipmi_component.runtime_metrics;

    if(0 == orcm_sensor_base_runtime_metrics_active_label_count(metrics_obj) &&
       !orcm_sensor_base_runtime_metrics_do_collect(metrics_obj, NULL)) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    mca_sensor_ipmi_component.diagnostics |= 0x1;

    /* prep the buffer to collect the data */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    /* pack our component name - 1*/
    ipmi = "ipmi";
    ORCM_ON_NULL_RETURN(ipmi);

    rc = opal_dss.pack(&data, &ipmi, 1, OPAL_STRING);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    /* Begin sampling known nodes from here */
    host_count = opal_list_get_size(&sensor_active_hosts);
    if (0 == host_count) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "No IPMI Device available for sampling");
        OBJ_DESTRUCT(&data);
        return;
    }

    /* pack the numerical identifier for number of nodes */
    rc = opal_dss.pack(&data, &host_count, 1, OPAL_INT);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

    OPAL_LIST_FOREACH_SAFE(host, nxt, &sensor_active_hosts, orcm_sensor_hosts_t) {
        if(ORCM_SUCCESS != (rc = collect_ipmi_subsequent_data_for_host(&data, host))) {
            opal_output(0, "WARNING: A problem occurred sampling host %s. ",
                            host->capsule.node.name);
        }
    }

    /* Pack the list into a buffer and pass it onto heartbeat */
    bptr = &data;
    rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
    ORCM_ON_FAILURE_GOTO(rc, cleanup);

cleanup:
    OBJ_DESTRUCT(&data);
    return;
}

void collect_ipmi_sample(orcm_sensor_sampler_t* sampler)
{
    if(1 == disable_ipmi) {
        return;
    }

    if(mca_sensor_ipmi_component.test) {
        generate_test_vector(sampler);
        mca_sensor_ipmi_component.diagnostics |= (mca_sensor_ipmi_component.collect_metrics?1:0);
    } else {
        collect_ipmi_subsequent_data(sampler);
    }
}

#define TEST_LABEL_COUNT 10
struct test_vector_type {
    char* label;
    float value;
    char* units;
};
struct test_vector_type2 {
    char* label;
    char* value;
};
static void generate_test_vector_for_host(char* host, opal_buffer_t* buffer)
{
    static const struct test_vector_type2 test_vector_info[5] = {
        { "bmcfwrev", "4.2" },
        { "ipmiver", "2.0" },
        { "manufacturer_id", "some_long_id" },
        { "sys_power_state", "ON" },
        { "dev_power_state", "ON" }
    };
    static const struct test_vector_type test_vector_data[5] = {
        { "Exhaust Temperature", 55.0, "C" },
        { "PSU 1 Power", 100.0, "W" },
        { "PSU 2 Power", 50.0, "W" },
        { "CPU FAN 1", 1200.0, "rpm" },
        { "CPU FAN 2", 800.0, "rpm" }
    };
    struct timeval time_stamp;
    opal_list_t* list = NULL;
    orcm_value_t* item = NULL;

    gettimeofday(&time_stamp, NULL);
    opal_dss.pack(buffer, &time_stamp, 1, OPAL_TIMEVAL);
    opal_dss.pack(buffer, &host, 1, OPAL_STRING);

    list = OBJ_NEW(opal_list_t);
    for(int i = 0; i < (TEST_LABEL_COUNT/2); ++i) {
        item = orcm_util_load_orcm_value(test_vector_info[i].label,
                                         (void*)test_vector_info[i].value,
                                         OPAL_STRING, "");
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(list, (opal_list_item_t*)item);
        item = NULL;
    }

    for(int i = 0; i < (TEST_LABEL_COUNT/2); ++i) {
        item = orcm_util_load_orcm_value(test_vector_data[i].label,
                                         (void*)&test_vector_data[i].value,
                                         OPAL_FLOAT, test_vector_data[i].units);
        ON_NULL_GOTO(item, cleanup);
        opal_list_append(list, (opal_list_item_t*)item);
        item = NULL;
    }

    orcm_sensor_pack_orcm_value_list(buffer, list);

cleanup:
    ORCM_RELEASE(list);
    return;
}

#define TEST_HOST_COUNT 4
static void generate_test_vector_inner(opal_buffer_t* buffer)
{
    static char* test_hosts[TEST_HOST_COUNT] = {
        "test_host_1",
        "test_host_2",
        "test_host_3",
        "test_host_4"
    };
    char* ipmi = "ipmi";
    int host_count = TEST_HOST_COUNT;
    opal_dss.pack(buffer, &ipmi, 1, OPAL_STRING);
    opal_dss.pack(buffer, &host_count, 1, OPAL_INT);
    for(int i = 0; i < host_count; ++i) {
        generate_test_vector_for_host(test_hosts[i], buffer);
    }
    return;
}

static void generate_test_vector(orcm_sensor_sampler_t* sampler)
{
    opal_buffer_t buffer;
    opal_buffer_t* bptr = &buffer;

    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    generate_test_vector_inner(bptr);
    opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
    OBJ_DESTRUCT(&buffer);
}

static void generate_test_vector_inv(opal_buffer_t *inventory_snapshot)
{
    static const char* sensor_inventory[TEST_LABEL_COUNT+4][2] = {
        { "bmc_ver", "4.2" },
        { "ipmi_ver", "2.0" },
        { "bb_serial", "some_long_id" },
        { "bb_vendor", "Intel Corporation" },
        { "sensor_ipmi_1", "bmcfwrev" },
        { "sensor_ipmi_2", "ipmiver" },
        { "sensor_ipmi_3", "manufacturer_id" },
        { "sensor_ipmi_4", "sys_power_state" },
        { "sensor_ipmi_5", "dev_power_state" },
        { "sensor_ipmi_6", "Exhaust Temperature" },
        { "sensor_ipmi_7", "PSU 1 Power" },
        { "sensor_ipmi_8", "PSU 2 Power" },
        { "sensor_ipmi_9", "CPU FAN 1" },
        { "sensor_ipmi_10", "CPU FAN 2" }
    };
    const char *comp = "ipmi";
    unsigned int tot_items = TEST_LABEL_COUNT+4;
    struct timeval time_stamp;

    ORCM_ON_NULL_RETURN(inventory_snapshot);

    opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING);
    gettimeofday(&time_stamp, NULL);
    opal_dss.pack(inventory_snapshot, &time_stamp, 1, OPAL_TIMEVAL);
    opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT);

    for(unsigned int i = 0; i < tot_items; ++i) {
        opal_dss.pack(inventory_snapshot, &(sensor_inventory[i][0]), 1, OPAL_STRING);
        opal_dss.pack(inventory_snapshot, &(sensor_inventory[i][1]), 1, OPAL_STRING);
    }

    return;
}

void orcm_sensor_ipmi_get_system_power_state(uchar in, char* str, int str_size)
{
    char in_r = in & 0x7F;
    switch(in_r) {
        case 0x0:   strncpy(str,"S0/G0",str_size-1); break;
        case 0x1:   strncpy(str,"S1",str_size-1); break;
        case 0x2:   strncpy(str,"S2",str_size-1); break;
        case 0x3:   strncpy(str,"S3",str_size-1); break;
        case 0x4:   strncpy(str,"S4",str_size-1); break;
        case 0x5:   strncpy(str,"S5/G2",str_size-1); break;
        case 0x6:   strncpy(str,"S4/S5",str_size-1); break;
        case 0x7:   strncpy(str,"G3",str_size-1); break;
        case 0x8:   strncpy(str,"sleeping",str_size-1); break;
        case 0x9:   strncpy(str,"G1 sleeping",str_size-1); break;
        case 0x0A:  strncpy(str,"S5 override",str_size-1); break;
        case 0x20:  strncpy(str,"Legacy On",str_size-1); break;
        case 0x21:  strncpy(str,"Legacy Off",str_size-1); break;
        case 0x2A:  strncpy(str,"Unknown",str_size-1); break;
        default:    strncpy(str,"Illegal",str_size-1); break;
    }
    str[str_size-1] = '\0';
}
void orcm_sensor_ipmi_get_device_power_state(uchar in, char* str, int str_size)
{
    char in_r = in & 0x7F;
    switch(in_r) {
        case 0x0:   strncpy(str,"D0",str_size-1); break;
        case 0x1:   strncpy(str,"D1",str_size-1); break;
        case 0x2:   strncpy(str,"D2",str_size-1); break;
        case 0x3:   strncpy(str,"D3",str_size-1); break;
        case 0x4:   strncpy(str,"Unknown",str_size-1); break;
        default:    strncpy(str,"Illegal",str_size-1); break;
    }
    str[str_size-1] = '\0';
}

void orcm_sensor_ipmi_get_device_id(ipmi_capsule_t *cap)
{
    int ret = 0;
    char addr[16];
    unsigned char idata[4], rdata[MAX_IPMI_RESPONSE];
    unsigned char ccode;
    int rlen = MAX_IPMI_RESPONSE;
    char fdebug = 0;
    device_id_t devid;
    char *error_string;

    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
    if(0 == ret)
    {
        ret = ipmi_cmd_mc(GET_DEVICE_ID, idata, 0, rdata, &rlen, &ccode, fdebug);
        if(0 == ret)
        {
            ipmi_close();
            memcpy(&devid.raw, rdata, sizeof(devid));

            /*  Pack the BMC FW Rev */
            snprintf(cap->prop.bmc_rev, sizeof(cap->prop.bmc_rev),
                    "%x.%x", devid.bits.fw_rev_1&0x7F, devid.bits.fw_rev_2&0xFF);

            /*  Pack the IPMI VER */
            snprintf(cap->prop.ipmi_ver,sizeof(cap->prop.ipmi_ver),
                    "%x.%x", devid.bits.ipmi_ver&0xF, devid.bits.ipmi_ver&0xF0);

            /*  Pack the Manufacturer ID */
            snprintf(cap->prop.man_id, sizeof(cap->prop.man_id),
                    "%x%02x%02x", (devid.bits.manufacturer_id[2]&0x0f), devid.bits.manufacturer_id[1], devid.bits.manufacturer_id[0]);
        } else {
            /*disable_ipmi = 1;*/
            error_string = decode_rv(ret);
            orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-cmd-mc-fail",
                       true, orte_process_info.nodename,
                       orte_process_info.nodename, cap->node.bmc_ip,
                       cap->node.user, cap->node.pasw, cap->node.auth,
                       cap->node.priv, cap->node.ciph, error_string);
        }
    } else {
        error_string = decode_rv(ret);
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-set-lan-fail",
                       true, orte_process_info.nodename,
                       orte_process_info.nodename, cap->node.bmc_ip,
                       cap->node.user, cap->node.pasw, cap->node.auth,
                       cap->node.priv, cap->node.ciph, error_string);
    }

}

void orcm_sensor_ipmi_get_power_states(ipmi_capsule_t *cap)
{
    char addr[16];
    int ret = 0;
    unsigned char idata[4], rdata[MAX_IPMI_RESPONSE];
    unsigned char ccode;
    int rlen = MAX_IPMI_RESPONSE;
    char fdebug = 0;
    acpi_power_state_t pwr_state;
    char sys_pwr_state_str[16], dev_pwr_state_str[16];
    char *error_string;

    memset(rdata,0xff,sizeof(rdata));
    memset(idata,0xff,sizeof(idata));
    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
    if(0 == ret)
    {
        ret = ipmi_cmd_mc(GET_ACPI_POWER, idata, 0, rdata, &rlen, &ccode, fdebug);
        if(0 == ret)
        {
            ipmi_close();
            memcpy(&pwr_state.raw, rdata, sizeof(pwr_state));
            orcm_sensor_ipmi_get_system_power_state(pwr_state.bits.sys_power_state, sys_pwr_state_str, sizeof(sys_pwr_state_str));
            orcm_sensor_ipmi_get_device_power_state(pwr_state.bits.dev_power_state, dev_pwr_state_str, sizeof(dev_pwr_state_str));
            /* Copy all retrieved information in a global buffer */
            memcpy(cap->prop.sys_power_state,sys_pwr_state_str,MIN(sizeof(sys_pwr_state_str),sizeof(cap->prop.sys_power_state)));
            memcpy(cap->prop.dev_power_state,dev_pwr_state_str,MIN(sizeof(dev_pwr_state_str),sizeof(cap->prop.dev_power_state)));
        } else {
            error_string = decode_rv(ret);
            orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-cmd-mc-fail",
                       true, orte_process_info.nodename,
                       orte_process_info.nodename, cap->node.bmc_ip,
                       cap->node.user, cap->node.pasw, cap->node.auth,
                       cap->node.priv, cap->node.ciph, error_string);
        }
    } else {
        error_string = decode_rv(ret);
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-set-lan-fail",
                       true, orte_process_info.nodename,
                       orte_process_info.nodename, cap->node.bmc_ip,
                       cap->node.user, cap->node.pasw, cap->node.auth,
                       cap->node.priv, cap->node.ciph, error_string);
    }
}

bool does_sensor_group_match_sensor_name(char* sensor_group, char* sensor_name)
{
    ORCM_ON_NULL_RETURN_ERROR(sensor_group, false);
    ORCM_ON_NULL_RETURN_ERROR(sensor_name, false);

    if(1 == strlen(sensor_group) && '*' == sensor_group[0]) {
        return true;
    } else {
        return (NULL != strcasestr(sensor_name, sensor_group));
    }
}

void orcm_sensor_ipmi_get_sensor_reading(ipmi_capsule_t *cap)
{
    char addr[16];
    int ret = 0;

    char tag[17];
    unsigned char snum = 0;
    unsigned char reading[4];       /* Stores the individual sensor reading */
    double val;
    char *typestr;                  /* Stores the individual sensor unit */
    unsigned short int id = 0;
    unsigned char sdrbuf[SDR_SZ];
    unsigned char *sdrlist;
    char *error_string;
    int sensor_count = 0;

    /* BEGIN: Gathering SDRs */
    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
    if(ret)
    {
        error_string = decode_rv(ret);
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-set-lan-fail",
                           true, orte_process_info.nodename,
                           cap->node.name, cap->node.bmc_ip,
                           cap->node.user, cap->node.pasw, cap->node.auth,
                           cap->node.priv, cap->node.ciph, error_string);
        return;
    } else {
        ret = get_sdr_cache(&sdrlist);
        if (ret) {
            error_string = decode_rv(ret);
            orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-get-sdr-fail",
                           true, orte_process_info.nodename,
                           cap->node.bmc_ip, cap->node.bmc_ip,
                           cap->node.user, cap->node.pasw, cap->node.auth,
                           cap->node.priv, cap->node.ciph, error_string);
            return;
        } else {
            while(find_sdr_next(sdrbuf,sdrlist,id) == 0)
            {
                id = sdrbuf[0] + (sdrbuf[1] << 8); /* this SDR id */
                if (sdrbuf[3] != 0x01) continue; /* full SDR */
                strncpy(tag,(char *)&sdrbuf[48],sizeof(tag)-1);
                tag[(int)(((SDR01REC *)sdrbuf)->id_strlen & 0x1f)] = 0; /* IPMI structure caps copy to 16 characters */
                snum = sdrbuf[7];
                ret = GetSensorReading(snum, sdrbuf, reading);
                if (ret == 0)
                {
                    val = RawToFloat(reading[0], sdrbuf);
                    typestr = get_unit_type( sdrbuf[20], sdrbuf[21], sdrbuf[22],0);
                    if(orcm_sensor_ipmi_label_found(tag))
                    {
                        /*  Pack the Sensor Metric */
                        cap->prop.collection_metrics[sensor_count]=val;
                        strncpy(cap->prop.collection_metrics_units[sensor_count],typestr,sizeof(cap->prop.collection_metrics_units[sensor_count])-1);
                        cap->prop.collection_metrics_units[sensor_count][sizeof(cap->prop.collection_metrics_units[sensor_count])-1] = '\0';
                        strncpy(cap->prop.metric_label[sensor_count],tag,sizeof(cap->prop.metric_label[sensor_count])-1);
                        cap->prop.metric_label[sensor_count][sizeof(cap->prop.metric_label[sensor_count])-1] = '\0';
                        sensor_count++;
                    } else if(NULL!=mca_sensor_ipmi_component.sensor_group) {
                        if(true == does_sensor_group_match_sensor_name(mca_sensor_ipmi_component.sensor_group, tag))
                        {
                            /*  Pack the Sensor Metric */
                            cap->prop.collection_metrics[sensor_count]=val;
                            strncpy(cap->prop.collection_metrics_units[sensor_count],typestr,sizeof(cap->prop.collection_metrics_units[sensor_count])-1);
                            cap->prop.collection_metrics_units[sensor_count][sizeof(cap->prop.collection_metrics_units[sensor_count])-1] = '\0';
                            strncpy(cap->prop.metric_label[sensor_count],tag,sizeof(cap->prop.metric_label[sensor_count])-1);
                            cap->prop.metric_label[sensor_count][sizeof(cap->prop.metric_label[sensor_count])-1] = '\0';
                            sensor_count++;
                        }
                    }
                    if (sensor_count == TOTAL_FLOAT_METRICS)
                    {
                        opal_output(0, "Max 'sensor' sampling reached for IPMI Plugin: %d",
                            sensor_count);
                        break;
                    }
                } else {
                    val = 0;
                    typestr = "na";
                }
                memset(sdrbuf,0,SDR_SZ);
            }
            free_sdr_cache(sdrlist);
            cap->prop.total_metrics = sensor_count;
        }
        ipmi_close();
        /* End: gathering SDRs */
    }
}

static int orcm_sensor_ipmi_get_sensor_inventory_list(opal_list_t *inventory_list)
{
    int rc = 0;
    orcm_sensor_hosts_t* host = NULL;
    orcm_sensor_hosts_t* nxt = NULL;

    OPAL_LIST_FOREACH_SAFE(host, nxt, &sensor_active_hosts, orcm_sensor_hosts_t) {
        rc |= get_sensor_inventory_list_from_node(inventory_list, &host->capsule);
        rc |= orcm_sensor_get_fru_inv(host);
        if(ORCM_SUCCESS != rc) {
            opal_output(0, "WARNING: A problem occurred while gathering inventory from host %s. ",
                            host->capsule.node.name);
        }
    }

    return ORCM_SUCCESS;
}

static int get_sensor_inventory_list_from_node(opal_list_t *inventory_list, ipmi_capsule_t *cap)
{
    int ret = 0;
    char addr[16];
    unsigned char *sdrlist = NULL;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Gathering local ipmi sensors for inventory");
    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);

    if (ret) {
        char *error_string;
        error_string = decode_rv(ret);
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-set-lan-fail",
                           true, orte_process_info.nodename,
                           cap->node.name, cap->node.bmc_ip,
                           cap->node.user, "*****", cap->node.auth,
                           cap->node.priv, cap->node.ciph, error_string);
        opal_output(0, "Failed to get sensor inventory data!");
        return ORCM_ERROR;
    }

    ret = get_sdr_cache(&sdrlist);
    if (ret) {
        char *error_string;

        error_string = decode_rv(ret);
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-get-sdr-fail",
                       true, orte_process_info.nodename,
                       cap->node.name, cap->node.bmc_ip,
                       cap->node.user, "*****", cap->node.auth,
                       cap->node.priv, cap->node.ciph, error_string);
        opal_output(0, "Failed to get sensor inventory data!");
        return ORCM_ERROR;
    } else {
        unsigned short int id = 0;
        unsigned char sdrbuf[SDR_SZ];
        int sensor_num = 1;
        char *key = NULL;
        opal_value_t *kv = NULL;

        while(0 == find_sdr_next(sdrbuf, sdrlist, id)) {
            id = sdrbuf[0] + (sdrbuf[1] << 8); /* this SDR id */
            if (0x01 != sdrbuf[3]) {
                continue; /* if not full SDR try next one */
            } else {
                char val[17]; /* 16 max chars plus EoS character (IPMI Spec) */
                int name_size = 0;

                strncpy(val, (char*)&sdrbuf[48], sizeof(val)-1);
                name_size = (int)(((SDR01REC*)sdrbuf)->id_strlen & 0x1f);
                val[MIN(name_size,sizeof(val)-1)] = '\0'; /* IPMI structure caps copy to 16 characters */
                asprintf(&key, "sensor_ipmi_%d", sensor_num);

                /* Store Inventory Key */
                kv = orcm_util_load_opal_value(NULL, key, OPAL_STRING);
                opal_list_append(inventory_list, &kv->super); /* List owns kv*/
                kv = NULL;

                /* Store Inventory Value */
                kv = orcm_util_load_opal_value(NULL, val, OPAL_STRING);
                opal_list_append(inventory_list, &kv->super); /* List owns kv*/
                kv = NULL;
                orcm_sensor_base_runtime_metrics_track(mca_sensor_ipmi_component.runtime_metrics, val);

                ++sensor_num; /* increment sensor number for key */
            }
        }
        free_sdr_cache(sdrlist);
        ipmi_close();

        /* Store node name*/
        kv = orcm_util_load_opal_value(NULL, "hostname", OPAL_STRING);
        opal_list_append(inventory_list, &kv->super); /* List owns kv*/
        kv = NULL;

        kv = orcm_util_load_opal_value(NULL, cap->node.name, OPAL_STRING);
        opal_list_append(inventory_list, &kv->super); /* List owns kv*/
        kv = NULL;

        return ORCM_SUCCESS;
    }
}

void orcm_sensor_sel_error_callback(int level, const char* msg)
{
    static char* level_strings[2] = {
        "ERROR",
        "INFO"
    };
    char* line;
    asprintf(&line, "%s: collecting IPMI SEL records: %s\n", (0 == level)?level_strings[0]:level_strings[1], msg);
    opal_output_verbose(level, orcm_sensor_base_framework.framework_output, line);
    free(line);
}

void orcm_sensor_sel_ras_event_callback(const char* event, const char* hostname, void* user_object)
{
    opal_list_t* record_list = (opal_list_t*)user_object;
    orcm_value_t* record = orcm_util_load_orcm_value("sel_event_record", (void*)event, OPAL_STRING, "");

    ORCM_ON_NULL_RETURN(record);
    opal_list_append(record_list, (opal_list_item_t*)record);
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "SEL Record Read: %s: %s", (char*)hostname, (char*)event);
}

void orcm_sensor_ipmi_get_sel_events(ipmi_capsule_t* capsule)
{
    process_node_sel_logs((const char*)capsule->node.name, (const char*)capsule->node.bmc_ip,
                          (const char*)capsule->node.user, (const char*)capsule->node.pasw,
                          mca_sensor_ipmi_component.sel_state_filename, orcm_sensor_sel_error_callback,
                          orcm_sensor_sel_ras_event_callback, (void*)capsule->prop.collected_sel_records);
}


int ipmi_enable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, true, sensor_specification);
}

int ipmi_disable_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_set(metrics, false, sensor_specification);
}

int ipmi_reset_sampling(const char* sensor_specification)
{
    void* metrics = mca_sensor_ipmi_component.runtime_metrics;
    return orcm_sensor_base_runtime_metrics_reset(metrics, sensor_specification);

}

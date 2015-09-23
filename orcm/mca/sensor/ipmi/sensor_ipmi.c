/*
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
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
#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "opal/runtime/opal_progress_threads.h"

#include "sensor_ipmi.h"
#include <../share/ipmiutil/isensor.h>

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
static void collect_sample(orcm_sensor_sampler_t *sampler);
static void ipmi_log(opal_buffer_t *buf);
static void ipmi_inventory_collect(opal_buffer_t *inventory_snapshot);
static void ipmi_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
static void ipmi_set_sample_rate(int sample_rate);
static void ipmi_get_sample_rate(int *sample_rate);
bool does_sensor_group_match_sensor_name(char* sensor_group, char* sensor_name);

int first_sample = 0;

char **sensor_list_token; /* 2D array storing multiple sensor keywords for collecting metrics */

opal_list_t sensor_active_hosts; /* Hosts list for collecting metrics */
opal_list_t ipmi_inventory_hosts; /* Hosts list for storing inventory details */
static orcm_sensor_sampler_t *ipmi_sampler = NULL;
static orcm_sensor_ipmi_t orcm_sensor_ipmi;

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
    trk->records = OBJ_NEW(opal_list_t);
}
static void inv_des(ipmi_inventory_t *trk)
{
    if(NULL != trk) {
        if(NULL != trk->records) {
            OPAL_LIST_RELEASE(trk->records);
        }
        if(NULL != trk->nodename) {
            free(trk->nodename);
        }
    }
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
    ipmi_get_sample_rate
};

/* local variables */
static opal_buffer_t test_vector;
static bool log_enabled = true;
static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, opal_list_t *ret, void *cbdata);
static void generate_test_vector(opal_buffer_t *v);
static void generate_test_vector_inv(opal_buffer_t *inventory_snapshot);

char ipmi_inv_tv[5][2][30] = {
{"bmc_ver","9.9"},
{"ipmi_ver","8.8"},
{"bb_serial","TV_BbSer"},
{"bb_vendor","TV_BbVen"},
{"bb_manufactured_date","TV_MaufDat"}};

static int init(void)
{
    int rc;
    disable_ipmi = 0;
    char user[16];

    OBJ_CONSTRUCT(&sensor_active_hosts, opal_list_t);
    OBJ_CONSTRUCT(&ipmi_inventory_hosts, opal_list_t);
    cur_host = OBJ_NEW(orcm_sensor_hosts_t);
    if (mca_sensor_ipmi_component.test) {
        /* generate test vector */
        OBJ_CONSTRUCT(&test_vector, opal_buffer_t);
        generate_test_vector(&test_vector);
        return OPAL_SUCCESS;
    }

    /* Verify if user has root privileges, if not do not try to read BMC Credentials*/
    getlogin_r(user, sizeof(user));
    if(geteuid() != 0) {
        orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-not-superuser",
                       true, orte_process_info.nodename, user);
        return ORCM_ERROR;
    }

    rc = orcm_sensor_ipmi_get_bmc_cred(cur_host);
    if(rc != ORCM_SUCCESS) {
        opal_output(0, "Unable to collect the current host details");
        return ORCM_ERROR;
    }

    return OPAL_SUCCESS;
}

static void finalize(void)
{
    OPAL_LIST_DESTRUCT(&sensor_active_hosts);
    OPAL_LIST_DESTRUCT(&ipmi_inventory_hosts);
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
       collect_sample(sampler);
    }

}

static void ipmi_log(opal_buffer_t *sample)
{
    char *hostname, *sample_item, *sample_name, *sample_unit;
    char nodename[64], hostip[16], bmcip[16], baseboard_manuf_date[11], baseboard_manufacturer[30], baseboard_name[16], baseboard_serial[16], baseboard_part[16];
    float float_item;
    unsigned uint_item;
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;
    struct timeval sampletime;
    orcm_metric_value_t *sensor_metric;

    int host_count;
    if (!log_enabled) {
        return;
    }
    if(disable_ipmi == 1)
        return;

    /* Unpack the host_count identifer */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &host_count, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return;
    } else {
        if(host_count==0) {
            /*New Node is getting added */

            /* sample time - 1b */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL))) {
                ORTE_ERROR_LOG(rc);
                return;
            }

            /* Unpack the node_name - 2 */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            strncpy(nodename,hostname,sizeof(nodename)-1);
            nodename[sizeof(nodename)-1] = '\0';
            opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
                                "IPMI_LOG -> Node %s not found; Logging credentials", hostname);
            free(hostname);

            /* Unpack the host_ip - 3a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == hostname) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked host_ip(3a): %s",hostname);
            strncpy(hostip,hostname,sizeof(hostip)-1);
            hostip[sizeof(hostip)-1] = '\0';
            free(hostname);

            /* Unpack the bmcip - 4a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == hostname) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked BMC_IP(4a): %s",hostname);
            strncpy(bmcip,hostname,sizeof(bmcip)-1);
            bmcip[sizeof(bmcip)-1] = '\0';
            free(hostname);

            /* Unpack the Baseboard Manufacture Date - 5a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == sample_item) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked Baseboard Manufacture Date(5a): %s", sample_item);
            strncpy(baseboard_manuf_date,sample_item,sizeof(baseboard_manuf_date)-1);
            baseboard_manuf_date[sizeof(baseboard_manuf_date)-1] = '\0';
            free(sample_item);

            /* Unpack the Baseboard Manufacturer Name - 6a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == sample_item) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked Baseboard Manufacturer Name(6a): %s", sample_item);
            strncpy(baseboard_manufacturer,sample_item,(sizeof(baseboard_manufacturer)-1));
            baseboard_manufacturer[sizeof(baseboard_manufacturer)-1] = '\0';
            free(sample_item);

            /* Unpack the Baseboard Product Name - 7a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == sample_item) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked Baseboard Product Name(7a): %s", sample_item);
            strncpy(baseboard_name,sample_item,(sizeof(baseboard_name)-1));
            baseboard_name[sizeof(baseboard_name)-1] = '\0';
            free(sample_item);

            /* Unpack the Baseboard Serial Number - 8a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == sample_item) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked Baseboard Serial Number(8a): %s", sample_item);
            strncpy(baseboard_serial,sample_item,(sizeof(baseboard_serial)-1));
            baseboard_serial[sizeof(baseboard_serial)-1] = '\0';
            free(sample_item);

            /* Unpack the Baseboard Part Number - 9a */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            if (NULL == sample_item) {
                ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
                return;
            }
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Unpacked Baseboard Part Number(9a): %s", sample_item);
            strncpy(baseboard_part,sample_item,(sizeof(baseboard_part)-1));
            baseboard_part[sizeof(baseboard_part)-1] = '\0';
            free(sample_item);

            /* Add the node only if it has not been added previously, for the
             * off chance that the compute node daemon was started once before,
             * and after running for sometime was killed
             * VINFIX: Eventually, this node which is already present and is
             * re-started has to be removed first, and then added again afresh,
             * just so that we update our list with the latest credentials
             */
            if(ORCM_ERR_NOT_FOUND == orcm_sensor_ipmi_found(nodename, &sensor_active_hosts))
            {
                if(ORCM_SUCCESS != orcm_sensor_ipmi_addhost(nodename, hostip, bmcip, &sensor_active_hosts)) /* Add the node to the slave list of the aggregator */
                {
                    opal_output(0,"Unable to add the new host! Try restarting ORCM");
                    orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-addhost-fail",
                           true, orte_process_info.nodename, nodename);
                    return;
                }
            } else {
                opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
                                    "Node already populated; Not going be added again");
            }
            /* Log the static information to database */
            /* @VINFIX: Currently will log into the same database as sensor data
             * But will eventually get moved to a different database (read
             * Inventory)
             */
            vals = OBJ_NEW(opal_list_t);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("ctime");
            kv->type = OPAL_TIMEVAL;
            kv->data.tv = sampletime;
            opal_list_append(vals, &kv->super);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup("hostname");
            kv->type = OPAL_STRING;
            kv->data.string = strdup(nodename);
            opal_list_append(vals, &kv->super);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

            kv = OBJ_NEW(opal_value_t);
            if (NULL == kv) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            kv->key = strdup("data_group");
            kv->type = OPAL_STRING;
            kv->data.string = strdup("ipmi");
            opal_list_append(vals, &kv->super);

             /* Add Baseboard manufacture date */
            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup("BBmanuf_date");
            sensor_metric->value.type = OPAL_STRING;
            sensor_metric->value.data.string = strdup(baseboard_manuf_date);
            sensor_metric->units = NULL;
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

             /* Add Baseboard manufacturer name */
            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup("BBmanuf");
            sensor_metric->value.type = OPAL_STRING;
            sensor_metric->value.data.string = strdup(baseboard_manufacturer);
            sensor_metric->units = NULL;
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

             /* Add Baseboard product name */
            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup("BBname");
            sensor_metric->value.type = OPAL_STRING;
            sensor_metric->value.data.string = strdup(baseboard_name);
            sensor_metric->units = NULL;
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

            /* Add Baseboard serial number */
            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup("BBserial");
            sensor_metric->value.type = OPAL_STRING;
            sensor_metric->value.data.string = strdup(baseboard_serial);
            sensor_metric->units = NULL;
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

            /* Add Baseboard part number */
            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup("BBpart");
            sensor_metric->value.type = OPAL_STRING;
            sensor_metric->value.data.string = strdup(baseboard_part);
            sensor_metric->units = NULL;
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked NodeName: %s", nodename);

            /* Send the unpacked data for one Node */
            /* store it */
            if (0 <= orcm_sensor_base.dbhandle) {
                orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_ENV_DATA, vals, NULL, mycleanup, NULL);
            } else {
                OPAL_LIST_RELEASE(vals);
            }
            return;
        } else {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                "IPMI_LOG -> Node Found; Logging metrics");
        }
    }
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Total Samples to be unpacked: %d", host_count);

    /* START UNPACKING THE DATA and Store it in a opal_list_t item. */
    for(int count = 0; count < host_count; count++)
    {
        vals = OBJ_NEW(opal_list_t);

        /* sample time - 2 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_TIMEVAL))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_TIMEVAL;
        kv->data.tv = sampletime;
        opal_list_append(vals, &kv->super);

        /* Unpack the node_name - 3 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("hostname");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked NodeName: %s", hostname);
        strncpy(nodename,hostname,sizeof(nodename)-1);
        nodename[sizeof(nodename)-1] = '\0';
        free(hostname);

        /* Pack sensor name */
        kv = OBJ_NEW(opal_value_t);
        if (NULL == kv) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        kv->key = strdup("data_group");
        kv->type = OPAL_STRING;
        kv->data.string = strdup("ipmi");
        opal_list_append(vals, &kv->super);

        /* BMC FW REV - 4 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup("bmcfwrev");
        sensor_metric->value.type = OPAL_STRING;
        sensor_metric->value.data.string = strdup(sample_item);
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked bmcfwrev: %s", sample_item);
        free(sample_item);

        /* IPMI VER - 5 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup("ipmiver");
        sensor_metric->value.type = OPAL_STRING;
        sensor_metric->value.data.string = strdup(sample_item);
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked ipmiver: %s", sample_item);
        free(sample_item);

        /* Manufacturer ID - 6 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup("manufacturer_id");
        sensor_metric->value.type = OPAL_STRING;
        sensor_metric->value.data.string = strdup(sample_item);
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked MANUF-ID: %s", sample_item);
        free(sample_item);

        /* System Power State - 7 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup("sys_power_state");
        sensor_metric->value.type = OPAL_STRING;
        sensor_metric->value.data.string = strdup(sample_item);
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        free(sample_item);

        /* Device Power State - 8 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        sensor_metric = OBJ_NEW(orcm_metric_value_t);
        if (NULL == sensor_metric) {
            ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
            return;
        }
        sensor_metric->value.key = strdup("dev_power_state");
        sensor_metric->value.type = OPAL_STRING;
        sensor_metric->value.data.string = strdup(sample_item);
        sensor_metric->units = NULL;
        opal_list_append(vals, (opal_list_item_t *)sensor_metric);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked DEV_PSTATE: %s", sample_item);
        free(sample_item);

        /* Total BMC sensor Metrics sampled - 9 (Not necessary for db_store) */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &uint_item, &n, OPAL_UINT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        /* Log All non-string metrics here */
        for(unsigned int count_metrics=0;count_metrics<uint_item;count_metrics++)
        {
            /* Metric Name */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_name, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }

            /* Metric Value*/
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(rc);
                return;
            }

            /* Metric Units */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_unit, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }

            sensor_metric = OBJ_NEW(orcm_metric_value_t);
            if (NULL == sensor_metric) {
                ORTE_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
                return;
            }
            sensor_metric->value.key = strdup(sample_name);
            sensor_metric->value.type = OPAL_FLOAT;
            sensor_metric->value.data.fval = float_item;
            sensor_metric->units = strdup(sample_unit);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked %s: %f", sample_name, float_item);
            opal_list_append(vals, (opal_list_item_t *)sensor_metric);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "PACKED DATA: %s:%f", sensor_metric->value.key, sensor_metric->value.data.fval);
            free(sample_name);
            free(sample_unit);
        }
        /* Send the unpacked data for one Node */
        /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_ENV_DATA, vals, NULL, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(vals);
        }
    }
}

static void ipmi_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int rc;
    unsigned int tot_items = 5;
    char *comp = strdup("ipmi");

    if (mca_sensor_ipmi_component.test) {
        /* generate test vector */
        generate_test_vector_inv(inventory_snapshot);
        return;
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        free(comp);
        return;
    }
    free(comp);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    comp = "bmc_ver";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    comp = cur_host->capsule.prop.bmc_rev;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    comp = "ipmi_ver";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    comp = cur_host->capsule.prop.ipmi_ver;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    comp = "bb_serial";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    comp = cur_host->capsule.prop.baseboard_serial;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    comp = "bb_vendor";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    comp = cur_host->capsule.prop.baseboard_manufacturer;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    comp = "bb_manufactured_date";
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    comp = cur_host->capsule.prop.baseboard_manuf_date;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
}

int orcm_sensor_ipmi_get_bmc_cred(orcm_sensor_hosts_t *host)
{
    unsigned char idata[4], rdata[20];
    unsigned char ccode;
    char bmc_ip[16];
    int rlen = 20;
    int ret = 0;
    char *error_string;
    device_id_t devid;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "RETRIEVING LAN CREDENTIALS");
    strncpy(host->capsule.node.host_ip,"CUR_HOST_IP",sizeof(host->capsule.node.host_ip)-1);
    host->capsule.node.host_ip[sizeof(host->capsule.node.host_ip)-1] = '\0';

    /* This IPMI call reading the BMC's IP address runs through the
     *  ipmi/imb/KCS driver
     */
    memset(idata,0x00,sizeof(idata));

    /* Read IP Address - Ref Table 23-* LAN Config Parameters of
     * IPMI v2 Rev 1.1
     */
    idata[1] = GET_BMC_IP_CMD;
    for(idata[0] = 0; idata[0]<16;idata[0]++)
    {
        ret = ipmi_cmd(GET_LAN_CONFIG, idata, 4, rdata, &rlen,&ccode, 0);
        if(ret)
        {
            error_string = decode_rv(ret);
            if (ERR_NO_DRV == ret) {
                orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-cmd-fail",
                                true, orte_process_info.nodename, error_string);
                return ORCM_ERROR;
            } else {
                rlen=20;
                continue;
            }
        }
        ipmi_close();
        if(ccode == 0)
        {
            sprintf(bmc_ip,"%d.%d.%d.%d",rdata[1], rdata[2], rdata[3], rdata[4]);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "RETRIEVED BMC's IP ADDRESS: %s",bmc_ip);
            strncpy(host->capsule.node.bmc_ip,bmc_ip,sizeof(host->capsule.node.bmc_ip)-1);
            host->capsule.node.bmc_ip[sizeof(host->capsule.node.bmc_ip)-1] = '\0';
            orcm_sensor_get_fru_inv(host);

            /* Get the DEVICE ID information as well */
            ret = ipmi_cmd(GET_DEVICE_ID, NULL, 0, rdata, &rlen, &ccode, 0);
            if(0 == ret)
            {
                ipmi_close();
                memcpy(&devid.raw, rdata, sizeof(devid));

                /*  Pack the BMC FW Rev */
                snprintf(host->capsule.prop.bmc_rev,sizeof(host->capsule.prop.bmc_rev),
                        "%x.%x", devid.bits.fw_rev_1&0x7F,devid.bits.fw_rev_2&0xFF);

                /*  Pack the IPMI VER */
                snprintf(host->capsule.prop.ipmi_ver, sizeof(host->capsule.prop.ipmi_ver),
                        "%x.%x", devid.bits.ipmi_ver&0xF,devid.bits.ipmi_ver&0xF0);
            } else {
                error_string = decode_rv(ret);
                opal_output(0,"Unable to collect IPMI Device ID information: %s",error_string);
            }

            return ORCM_SUCCESS;
        } else {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "Received a non-zero ccode: %d, relen:%d", ccode, rlen);
        }
        rlen=20;
    }
    return ORCM_ERROR;
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
    if (NULL == time_info) {
        return -1;
    }
    else {
        strftime(manuf_date,10,"%x",time_info);
    }

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

    if (NULL == board_manuf) {
        return -1;
    }

    for(i = 0; i < board_manuf_length; i++){
        board_manuf[i] = rdata[fru_offset + BOARD_INFO_DATA_START + 1 + i];
    }

    board_manuf[i] = '\0';
    strncpy(host->capsule.prop.baseboard_manufacturer, board_manuf, sizeof(host->capsule.prop.baseboard_manufacturer)-1);
    host->capsule.prop.baseboard_manufacturer[sizeof(host->capsule.prop.baseboard_manufacturer)-1] = '\0';

    free(board_manuf);
    return board_manuf_length;
}

int orcm_sensor_ipmi_get_product_name(unsigned char fru_offset, unsigned char *rdata, orcm_sensor_hosts_t *host)
{
    int i=0;
    unsigned char board_product_length; /*holds the length (in bytes) of board product name*/
    char *board_product_name; /*holds board product name*/

    board_product_length = rdata[fru_offset + BOARD_INFO_DATA_START] & 0x3f;
    board_product_name = (char*) malloc (board_product_length + 1); /* + 1 for the Null Character */

    if (NULL == board_product_name) {
        return -1;
    }

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

    if (NULL == board_serial_num) {
        return -1;
    }

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

    if (NULL == board_part_num) {
        return -1;
    }

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

    if (NULL == rdata) {
        return ORCM_ERROR;
    }

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

int orcm_sensor_ipmi_addhost(char *nodename, char *host_ip, char *bmc_ip, opal_list_t *host_list)
{
    orcm_sensor_hosts_t *newhost;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Adding New Node: %s, with BMC IP: %s", nodename, bmc_ip);

    newhost = OBJ_NEW(orcm_sensor_hosts_t);
    strncpy(newhost->capsule.node.name,nodename,sizeof(newhost->capsule.node.name)-1);
    newhost->capsule.node.name[sizeof(newhost->capsule.node.name)-1] = '\0';
    strncpy(newhost->capsule.node.host_ip,host_ip,sizeof(newhost->capsule.node.host_ip)-1);
    newhost->capsule.node.host_ip[sizeof(newhost->capsule.node.host_ip)-1] = '\0';
    strncpy(newhost->capsule.node.bmc_ip,bmc_ip,sizeof(newhost->capsule.node.bmc_ip)-1);
    newhost->capsule.node.bmc_ip[sizeof(newhost->capsule.node.bmc_ip)-1] = '\0';

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
    orcm_metric_value_t *newitem, *olditem;
    unsigned int count = 0, record_size = 0;
    /* @VINFIX: Need to come up with a clever way to implement comparision of different
     * ipmi inventory records
     */
    if((record_size = opal_list_get_size(newhost->records)) != opal_list_get_size(oldhost->records))
    {
        opal_output(0,"IPMI Inventory compare failed: Unequal item count;");
        return false;
    }
    newitem = (orcm_metric_value_t*)opal_list_get_first(newhost->records);
    olditem = (orcm_metric_value_t*)opal_list_get_first(oldhost->records);

    for(count = 0; count < record_size ; count++) {

        if((NULL == newitem) | (NULL == olditem)) {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                                "Unable to retrieve host record, will mark mismatch");
            return false;
        }
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
        newitem = (orcm_metric_value_t*)opal_list_get_next(newitem);
        olditem = (orcm_metric_value_t*)opal_list_get_next(olditem);
    }

    return true;
}
static void ipmi_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    char *inv, *inv_val;
    unsigned int tot_items;
    int rc, n;
    ipmi_inventory_t *newhost, *oldhost;
    orcm_metric_value_t *mkv, *mkv_copy;
    opal_value_t *kv;

    newhost = OBJ_NEW(ipmi_inventory_t);
    newhost->nodename = strdup(hostname);

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &tot_items, &n, OPAL_UINT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    while(tot_items > 0)
    {
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &inv_val, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        mkv = OBJ_NEW(orcm_metric_value_t);
        mkv->value.key = inv;

        if(!strncmp(inv,"bmc_ver",sizeof("bmc_ver")) | !strncmp(inv,"ipmi_ver",sizeof("ipmi_ver")))
        {
            mkv->value.type = OPAL_FLOAT;
            mkv->value.data.fval = strtof(inv_val,NULL);
        } else {
            mkv->value.type = OPAL_STRING;
            mkv->value.data.string = inv_val;
        }
        opal_list_append(newhost->records, (opal_list_item_t *)mkv);
        tot_items--;
    }

    if(NULL != (oldhost = found_inventory_host(hostname)))
    {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "Host '%s' Found, comparing values",hostname);
        if(false == compare_ipmi_record(newhost, oldhost))
        {
            opal_output(0,"IPMI Compare failed; Notify User; Update List; Update Database");
            OPAL_LIST_RELEASE(oldhost->records);
            oldhost->records=OBJ_NEW(opal_list_t);
            OPAL_LIST_FOREACH(mkv, newhost->records, orcm_metric_value_t) {
                mkv_copy = OBJ_NEW(orcm_metric_value_t);
                mkv_copy->value.key = strdup(mkv->value.key);

                if(!strncmp(mkv->value.key,"bmc_ver",sizeof("bmc_ver")) | !strncmp(mkv->value.key,"ipmi_ver",sizeof("ipmi_ver")))
                {
                    mkv_copy->value.type = OPAL_FLOAT;
                    mkv_copy->value.data.fval = mkv->value.data.fval;
                } else {
                    mkv_copy->value.type = OPAL_STRING;
                    mkv_copy->value.data.string = strdup(mkv->value.data.string);
                }
                opal_list_append(oldhost->records,(opal_list_item_t *)mkv_copy);
            }

            kv = orcm_util_load_opal_value("hostname", oldhost->nodename, OPAL_STRING);
            if (NULL == kv) {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                    "Unable to allocate data");
                return;
            }
            opal_list_prepend(oldhost->records, &kv->super);

            /* Send the collected inventory details to the database for storage */
            if (0 <= orcm_sensor_base.dbhandle) {
                orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA , oldhost->records, NULL, NULL, NULL);
            }
        } else {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "ipmi compare passed");
        }
        /* newhost structure can be destroyed after comparision with original list and update */
        OBJ_RELEASE(newhost);

    } else {
        /* Append the new node to the existing host list */
        opal_list_append(&ipmi_inventory_hosts, &newhost->super);
        
        kv = orcm_util_load_opal_value("hostname", newhost->nodename, OPAL_STRING);
        if (NULL == kv) {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Unable to allocate data");
            return;
        }
        opal_list_prepend(newhost->records, &kv->super);

        /* Send the collected inventory details to the database for storage */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA , newhost->records, NULL, NULL, NULL);
        }
    }
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
    collect_sample(sampler);
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

static void collect_sample(orcm_sensor_sampler_t *sampler)
{
    int rc;
    opal_buffer_t data, *bptr;
    char *ipmi;
    char *sample_str;
    int int_count=0;
    size_t host_count=0;
    static int timeout = 0;
    orcm_sensor_hosts_t *host, *nxt;
    struct timeval current_time;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor ipmi : collect_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));


    if (mca_sensor_ipmi_component.test) {
        /* just send the test vector */
        bptr = &test_vector;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        return;
    }
    if(disable_ipmi == 1)
        return;

    /* prep the buffer to collect the data */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    /* pack our component name - 1*/
    ipmi = strdup("ipmi");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &ipmi, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(ipmi);

    if(first_sample == 0 && timeout < 3)  /* The first time Sample is called, it shall retrieve/sample just the LAN credentials and pack it. */
    {
        timeout++;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "First Sample: Packing Credentials");

        /* pack the numerical identifier for number of nodes*/
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &host_count, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        gettimeofday(&current_time, NULL);

        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack our node name - 2a*/
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &orte_process_info.nodename, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        if(ORCM_SUCCESS != rc)
        {
            opal_output(0, "Lan Details Collection - Retry : %d", timeout);
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the host's IP Address - 3a*/
        sample_str = (char *)&cur_host->capsule.node.host_ip;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the BMC IP Address - 4a*/
        sample_str = (char *)&cur_host->capsule.node.bmc_ip;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Packing BMC IP: %s",sample_str);

        /* Pack the Baseboard Manufacture Date - 5a*/
        if (NULL == cur_host->capsule.prop.baseboard_manuf_date) {
            sample_str = "Board Manuf Date n/a";
        }
        else {
            sample_str = (char *)&cur_host->capsule.prop.baseboard_manuf_date;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the Baseboard Manufacturer Name - 6a*/
        if (NULL == cur_host->capsule.prop.baseboard_manufacturer) {
            sample_str = "Board Manuf n/a";
        }
        else {
            sample_str = (char *)&cur_host->capsule.prop.baseboard_manufacturer;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the Baseboard Product Name - 7a*/
        if (NULL == cur_host->capsule.prop.baseboard_name) {
            sample_str = "Board Name n/a";
        }
        else {
            sample_str = (char *)&cur_host->capsule.prop.baseboard_name;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the Baseboard Serial Number - 8a*/
        if (NULL == cur_host->capsule.prop.baseboard_serial) {
            sample_str = "Board Serial n/a";
        }
        else {
            sample_str = (char *)&cur_host->capsule.prop.baseboard_serial;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the Baseboard Part Number - 9a*/
        if (NULL == cur_host->capsule.prop.baseboard_part) {
            sample_str = "Board Part n/a";
        }
        else {
            sample_str = (char *)&cur_host->capsule.prop.baseboard_part;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the buffer, to pass to heartbeat - FINAL */
        bptr = &data;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
        if(!ORTE_PROC_IS_AGGREGATOR)
        {
            opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
                                "PROC_IS_COMPUTE_DAEMON");
            disable_ipmi = 1;
        } else {
            opal_output_verbose(5,orcm_sensor_base_framework.framework_output,
                                "PROC_IS_AGGREGATOR");
        }
        first_sample = 1;

        return;
    } /* End packing BMC credentials*/

    /* Begin sampling known nodes from here */
    host_count = opal_list_get_size(&sensor_active_hosts);
    if (0 == host_count) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "No IPMI Device available for sampling");
        OBJ_DESTRUCT(&data);
        return;
    }
    /* pack the numerical identifier for number of nodes*/
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &host_count, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    /* Loop through each host from the host list*/
    OPAL_LIST_FOREACH_SAFE(host, nxt, &sensor_active_hosts, orcm_sensor_hosts_t) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Scanning metrics from node: %s",host->capsule.node.name);
        /* Clear all memory for the ipmi_capsule */
        memset(&(host->capsule.prop), '\0', sizeof(host->capsule.prop));

        /* If the bmc username was passed as an mca parameter, set it. */
        if (NULL != mca_sensor_ipmi_component.bmc_username) {
            strncpy(host->capsule.node.user, mca_sensor_ipmi_component.bmc_username, sizeof(host->capsule.node.user)-1);
            host->capsule.node.user[sizeof(host->capsule.node.user)-1] = '\0';
        } else {
            strncpy(host->capsule.node.user, "root", sizeof(host->capsule.node.user)-1);
            host->capsule.node.user[sizeof(host->capsule.node.user)-1] = '\0';
        }

        /*
        If the bmc password was passed as an mca parameter, set it.
        Otherwise, leave it as null.
        */
        if (NULL != mca_sensor_ipmi_component.bmc_password) {
            strncpy(host->capsule.node.pasw, mca_sensor_ipmi_component.bmc_password, sizeof(host->capsule.node.pasw)-1);
            host->capsule.node.pasw[sizeof(host->capsule.node.pasw)-1] = '\0';
        }

        host->capsule.node.auth = IPMI_SESSION_AUTHTYPE_PASSWORD;
        host->capsule.node.priv = IPMI_PRIV_LEVEL_ADMIN;
        host->capsule.node.ciph = 3; /* Cipher suite No. 3 */

        /* Running a sample for a Node */
        orcm_sensor_ipmi_get_device_id(&host->capsule);
        orcm_sensor_ipmi_get_power_states(&host->capsule);
        orcm_sensor_ipmi_get_sensor_reading(&host->capsule);

        gettimeofday(&current_time, NULL);

        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &current_time, 1, OPAL_TIMEVAL))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the nodeName - 3 */
        sample_str = (char *)&host->capsule.node.name;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** NodeName: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the BMC FW Rev  - 4 */
        sample_str = (char *)&host->capsule.prop.bmc_rev;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** bmcrev: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the IPMI VER - 5 */
        sample_str = (char *)&host->capsule.prop.ipmi_ver;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** IPMIVER: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the Manufacturer ID - 6 */
        sample_str = (char *)&host->capsule.prop.man_id;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** MANUF-ID: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the System Power State - 7 */
        sample_str = (char *)&host->capsule.prop.sys_power_state;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** SYS_PSTATE: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the Device Power State - 8 */
        sample_str = (char *)&host->capsule.prop.dev_power_state;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** DEV_PSTATE: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /* Pack the total Number of Metrics Sampled */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &host->capsule.prop.total_metrics, 1, OPAL_UINT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }

        /*  Pack the non-string metrics and their units - 11-> END */
        for(int count_metrics=0;count_metrics<host->capsule.prop.total_metrics;count_metrics++)
        {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "***** %s: %f",host->capsule.prop.metric_label[count_metrics], host->capsule.prop.collection_metrics[count_metrics]);

            sample_str = (char *)&host->capsule.prop.metric_label[count_metrics];
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }

            if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &host->capsule.prop.collection_metrics[count_metrics], 1, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }

            sample_str = (char *)&host->capsule.prop.collection_metrics_units[count_metrics];
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }
        }
    }
    /* Pack the list into a buffer and pass it onto heartbeat */
    bptr = &data;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    OBJ_DESTRUCT(&data);
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Total nodes sampled: %d",int_count);
    /* this is currently a no-op */
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "IPMI sensors just got implemented! ----------- ;)");
}

static void mycleanup(int dbhandle, int status, opal_list_t *kvs,
                      opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void generate_test_vector(opal_buffer_t *v)
{

}

static void generate_test_vector_inv(opal_buffer_t *inventory_snapshot)
{
    char *comp;
    unsigned int tot_items = 5;
    int rc;
    if(NULL != inventory_snapshot)
    {

        comp = strdup("ipmi");
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        free(comp);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &tot_items, 1, OPAL_UINT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        while(tot_items > 0)
        {
            tot_items--;
            comp = ipmi_inv_tv[tot_items][0];
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            comp = ipmi_inv_tv[tot_items][1];
            if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
        }
    }
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
    if(NULL == sensor_group || NULL == sensor_name) {
        return false;
    }
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
                           orte_process_info.nodename, cap->node.bmc_ip,
                           cap->node.user, cap->node.pasw, cap->node.auth,
                           cap->node.priv, cap->node.ciph, error_string);
        return;
    } else {
        ret = get_sdr_cache(&sdrlist);
        if (ret) {
            error_string = decode_rv(ret);
            orte_show_help("help-orcm-sensor-ipmi.txt", "ipmi-get-sdr-fail",
                           true, orte_process_info.nodename,
                           orte_process_info.nodename, cap->node.bmc_ip,
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
                    } else if(NULL!=mca_sensor_ipmi_component.sensor_group)
                    {
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

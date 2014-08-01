/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
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

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_ipmi.h"

#include <string.h>

#include "orte/mca/errmgr/errmgr.h"
#include "orcm/mca/db/db.h"

#include "sensor_ipmi.h"
#include <pthread.h>

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void ipmi_sample(orcm_sensor_sampler_t *sampler);
static void ipmi_log(opal_buffer_t *buf);
int count_log = 0;
/* @VINFIX: Change this 'link' in a linked-list to OPAL_LIST */
orcm_sensor_hosts_t *active_hosts;


orcm_sensor_hosts_t cur_host; // Object to store the current node's access details

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_ipmi_module = {
    init,
    finalize,
    start,
    stop,
    ipmi_sample,
    ipmi_log
};

/* local variables */
static opal_buffer_t test_vector;
static void generate_test_vector(opal_buffer_t *v);
static time_t last_sample = 0;
static bool log_enabled = true;

static int init(void)
{
    /* int count = TOTAL_NODES;
    *  struct ipmi_properties *cur_node;
    */
    disable_ipmi = 0;

    /* opal_output(0,"IPMI Init - Create the linked list for all nodes"); */

    if (mca_sensor_ipmi_component.test) {
        /* generate test vector */
        OBJ_CONSTRUCT(&test_vector, opal_buffer_t);
        generate_test_vector(&test_vector);
        return OPAL_SUCCESS;
    }

    /* Currently NOT using the linked list to store the data, rather using a
       temporary variable to store and pack the data
    while(count > 0)
    {
        cur_node = (struct ipmi_properties*)malloc(sizeof(struct ipmi_properties ));
        cur_node->next = first_node;
        first_node = cur_node;
        count--;
    }
    */
    
    return OPAL_SUCCESS;
}

static void finalize(void)
{
    opal_output(0,"IPMI_FINALIZE");
}

/*Start monitoring of local processes */
static void start(orte_jobid_t jobid)
{
    opal_output(0,"IPMI Start");
    active_hosts = NULL;

    /* Initialize all metric identiers */
    strncpy(metric_string[PS1_USAGE],"ps1_usage",sizeof("ps1_usage"));
    strncpy(metric_string[PS1_TEMP],"ps1_temp",sizeof("ps1_temp"));
    strncpy(metric_string[PS2_USAGE],"ps2_usage",sizeof("ps2_usage"));
    strncpy(metric_string[PS2_TEMP],"ps2_temp",sizeof("ps2_temp"));
    strncpy(metric_string[FAN1_CPU_RPM],"cpu_fan_1",sizeof("cpu_fan_1"));
    strncpy(metric_string[FAN2_CPU_RPM],"cpu_fan_2",sizeof("cpu_fan_2"));
    strncpy(metric_string[FAN1_SYS_RPM],"sys_fan_1",sizeof("sys_fan_1"));
    strncpy(metric_string[FAN2_SYS_RPM],"sys_fan_2",sizeof("sys_fan_2"));

    return;
}


static void stop(orte_jobid_t jobid)
{
    opal_output(0,"IPMI Stop");
    count_log = 0;
    return;
}

int orcm_sensor_ipmi_get_bmc_cred(orcm_sensor_hosts_t *host)
{
    unsigned char idata[4], rdata[20];
	unsigned char ccode;
    char bmc_ip[16];
    int rlen = 20;
    int ret = 0;
    char *error_string;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "RETRIEVING LAN CREDENTIALS");
    strncpy(host->host_ipaddr,"CUR_HOST_IP",strlen("CUR_HOST_IP")+1);

    /* This IPMI call reading the BMC's IP address runs through the
     *  KCS driver
     */
    memset(idata,0x00,4);

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
            opal_output(0,"ipmi_cmd_mc RETURN CODE : %s \n", error_string);
            rlen=20;
            continue;
        }
        ipmi_close();
        if(ccode == 0)
        {
            sprintf(bmc_ip,"%d.%d.%d.%d",rdata[1], rdata[2], rdata[3], rdata[4]);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "RETRIEVED BMC's IP ADDRESS: %s",bmc_ip);
            strncpy(host->bmc_ipaddr,bmc_ip,strlen(bmc_ip)+1);
            strncpy(host->username,"CUR_USERNAME",strlen("CUR_USERNAME")+1);
            strncpy(host->password,"CUR_PASSWORD",strlen("CUR_PASSWORD")+1);
            return ORCM_SUCCESS;

            break;

        } else {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                        "Received a non-zero ccode: %d, relen:%d", ccode, rlen);
        }
        rlen=20;
    }
    return ORCM_ERROR;
     

}

/* int orcm_sensor_ipmi_found (char* nodename)
 * Return 0 if nodename matches an existing node
 * Return 1 if nodename doesn't match
 */
int orcm_sensor_ipmi_found(char *nodename)
{
    orcm_sensor_hosts_t *top = active_hosts;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node: %s", nodename);
    if(NULL == top)
    {
        return 0;
    } else {
        while(NULL != top){
            if(!strcmp(nodename,top->node_name))
            {
                return 1;
            } else {
                top = top->next;
            }
        }
        return 0;
    }    
}

/* int orcm_sensor_ipmi_counthosts (char* nodename)
 * Return total number of hosts added to the list
 */
unsigned int orcm_sensor_ipmi_counthosts(void)
{
    orcm_sensor_hosts_t *top = active_hosts;
    unsigned int count = 0;

    while(NULL != top){
        count++;
        top = top->next;
    }
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "FOUND HOSTS: %d", count);
    return count;
}


void orcm_sensor_ipmi_addhost(char *nodename, char *host_ip, char *bmc_ip)
{
    opal_output(0, "Adding New Node: %s, with BMC IP: %s", nodename, bmc_ip);
    orcm_sensor_hosts_t *top;
    if (NULL == active_hosts)
    {
        active_hosts = (orcm_sensor_hosts_t*)malloc(sizeof(orcm_sensor_hosts_t));
        /* Set the next link as NULL for the last member*/
        active_hosts->next = NULL;
    } else {
        top = (orcm_sensor_hosts_t*)malloc(sizeof(orcm_sensor_hosts_t));
        top->next = active_hosts;
        active_hosts = top;
    }

    strncpy(active_hosts->node_name,nodename,strlen(nodename)+1);
    strncpy(active_hosts->host_ipaddr,host_ip,strlen(host_ip)+1);
    strncpy(active_hosts->bmc_ipaddr,bmc_ip,strlen(bmc_ip)+1);
}

static void ipmi_sample(orcm_sensor_sampler_t *sampler)
{
    ipmi_capsule_t cap;
    int rc;
    //char node[TOTAL_NODES][16]={"192.168.0.102","192.168.0.100"};
    //char node_temp[TOTAL_NODES][16]={"HOST_1","HOST_2"};
    opal_buffer_t data, *bptr;
    char *ipmi;
    time_t now;
    double tdiff;
    char time_str[40];
    char *timestamp_str, *sample_str;
    struct tm *sample_time;
    orcm_sensor_hosts_t *top = active_hosts;
    int int_count=0;
    int sample_count=0;

    if (mca_sensor_ipmi_component.test) {
        /* just send the test vector */
        bptr = &test_vector;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        return;
    }
    if(disable_ipmi == 1)
        return;
    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
        "========================SAMPLE: %d===============================", count_log);

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

    strncpy(cap.node.user,"root", sizeof("root"));
    strncpy(cap.node.pasw,"knc@123", sizeof("knc@123"));

    cap.node.auth = IPMI_SESSION_AUTHTYPE_PASSWORD;
    cap.node.priv = IPMI_PRIV_LEVEL_ADMIN;
    cap.node.ciph = 3; /* Cipher suite No. 3 */
    if(count_log == 0)  // The first time Sample is called, it shall retrieve/sample just the LAN credentials and pack it.
    {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "First Sample: Packing Credentials");
        /* pack the numerical identifier for number of nodes*/
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_count, 1, OPAL_INT))) {
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

        rc = orcm_sensor_ipmi_get_bmc_cred(&cur_host);
        if(ORCM_SUCCESS != rc)
        {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
        /* Pack the host's IP Address - 3a*/
        sample_str = strdup(cur_host.host_ipaddr);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /* Pack the BMC IP Address - 4a*/
        sample_str = strdup(cur_host.bmc_ipaddr);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Packing BMC IP: %s",sample_str);
        free(sample_str);

        /* Pack the buffer, to pass to heartbeat - FINAL */
        bptr = &data;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
        if(!ORTE_PROC_IS_AGGREGATOR)
        {
            opal_output(0,"PROC_IS_COMPUTE_DAEMON");
            disable_ipmi = 1;
        } else {
            opal_output(0,"PROC_IS_AGGREGATOR");
        }
        return;
    }
    sample_count = orcm_sensor_ipmi_counthosts();
    /* pack the numerical identifier for number of nodes*/
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_count, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    for(int i =0;i<sample_count;i++)
    {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Scanning metrics from node: %s",top->node_name);
        /* Clear all memory for the ipmi_capsule */
        memset(&(cap.prop), '\0', sizeof(cap.prop));
        /* Enable/Disable the property/metric to be sampled */
        cap.capability[BMC_REV]         = 1;
        cap.capability[IPMI_VER]        = 1;
        cap.capability[SYS_POWER_STATE] = 1;
        cap.capability[DEV_POWER_STATE] = 1;
        cap.capability[PS1_USAGE]       = 1;
        cap.capability[PS1_TEMP]        = 1;
        cap.capability[PS2_USAGE]       = 1;
        cap.capability[PS2_TEMP]        = 1;
        cap.capability[FAN1_CPU_RPM]    = 1;
        cap.capability[FAN2_CPU_RPM]    = 1;
        cap.capability[FAN1_SYS_RPM]    = 1;
        cap.capability[FAN2_SYS_RPM]    = 1;

        if(NULL != top)
        {
            strncpy(cap.node.name, top->node_name, strlen(top->node_name)+1);
            strncpy(cap.node.bmc_ip, top->bmc_ipaddr, strlen(top->bmc_ipaddr)+1);
            strncpy(cap.node.host_ip, top->host_ipaddr, strlen(top->host_ipaddr)+1);
        }
        /* Running a sample for a Node */
        orcm_sensor_ipmi_exec_call(&cap);
        
        /*opal_output(0," *^*^*^* %s - %s - %s - %s - %s - %s", cap.node.bmc_ip
        *             , cap.prop.bmc_rev, cap.prop.ipmi_ver
        *             , cap.prop.sys_power_state, cap.prop.dev_power_state
        *             , cap.prop.ps1_usage);
        */

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

        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** TimeStamp: %s",timestamp_str);
        /* Pack the Sample Time - 2 */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &timestamp_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }
        free(timestamp_str);
 
        /* Pack the nodeName - 3 */
        sample_str = &cap.node.name;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** NodeName: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /* Pack the host_ip of each node - 4 */
        sample_str = &cap.node.host_ip;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** HostIP: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /* Pack BMC IP address - 5 */ 
        sample_str = &cap.node.bmc_ip;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the BMC FW Rev  - 6 */
        sample_str = &cap.prop.bmc_rev;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** bmcrev: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the IPMI VER - 7 */
        sample_str = &cap.prop.ipmi_ver;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** IPMIVER: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the Manufacturer ID - 8 */
        sample_str = &cap.prop.man_id;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** MANUF-ID: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the System Power State - 9 */
        sample_str = &cap.prop.sys_power_state;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** SYS_PSTATE: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the Device Power State - 10 */
        sample_str = &cap.prop.dev_power_state;
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "***** DEV_PSTATE: %s",sample_str);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }

        /*  Pack the non-string metrics and their units - 11-> END */
        for(int count_metrics=0;count_metrics<TOTAL_FLOAT_METRICS;count_metrics++)
        {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "***** %s: %f",metric_string[count_metrics], cap.prop.collection_metrics[count_metrics]);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.collection_metrics[count_metrics], 1, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }
            sample_str = &cap.prop.collection_metrics_units[count_metrics];
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                OBJ_DESTRUCT(&data);
                return;
            }
        }
        top=top->next;
    }
    /* xfer the data for transmission - need at least one prior sample before doing so */

    /* Pack the list into a buffer and pass it onto heartbeat */
    bptr = &data;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }

    OBJ_DESTRUCT(&data);
    last_sample = now;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Total nodes sampled: %d",int_count);
    /* this is currently a no-op */
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "IPMI sensors just got implemented! ----------- ;)");
}

static void mycleanup(int dbhandle, int status,
                      opal_list_t *kvs, void *cbdata)
{
    OPAL_LIST_RELEASE(kvs);
    if (ORTE_SUCCESS != status) {
        log_enabled = false;
    }
}

static void ipmi_log(opal_buffer_t *sample)
{
    char *hostname, *sampletime, *sample_item;
    char nodename[64], hostip[16], bmcip[16];
    float float_item;
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;
    char key_unit[40];
    int sample_count;
    if (!log_enabled) {
        return;
    }
    if(disable_ipmi == 1)
        return;
    opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
        "----------------------LOG: %d----------------------------", count_log);
    count_log++;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Count Log: %d", count_log);

    /* Unpack the sample_count identifer */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_count, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return;
    } else {
        if(sample_count==0) {
            /*New Node is getting added */

            /* Unpack the node_name - 2 */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            strncpy(nodename,hostname,strlen(hostname)+1);
            opal_output(0,"IPMI_LOG -> Node %s not found; Logging credentials", hostname);
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
            strncpy(hostip,hostname,strlen(hostname)+1);
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
            strncpy(bmcip,hostname,strlen(hostname)+1);
            
            /* Add the node only if it has not been added previously, for the 
             * off chance that the compute node daemon was started once before,
             * and after running for sometime was killed
             * VINFIX: Eventually, this node which is already present and is 
             * re-started has to be removed first, and then added again afresh,
             * just so that we update our list with the latest credentials
             */
            if(!orcm_sensor_ipmi_found(nodename))
            {
                orcm_sensor_ipmi_addhost(nodename, hostip, bmcip); /* Add the node to the slave list of the aggregator */
            } else {
                opal_output(0,"Node already populated; Not gonna be added again");
            }
            return;
        } else {
            opal_output_verbose(2, orcm_sensor_base_framework.framework_output,
                "IPMI_LOG -> Node Found; Logging metrics");
        }
    }
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Total Samples to be unpacked: %d", sample_count);

    /* START UNPACKING THE DATA and Store it in a opal_list_t item. */
    for(int count = 0; count < sample_count; count++) 
    {
        vals = OBJ_NEW(opal_list_t);

        /* sample time - 2 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sampletime, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if (NULL == sampletime) {
            ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sampletime);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked cTime: %s", sampletime);
        free(sampletime);


        /* Unpack the node_name - 3 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("nodename");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked NodeName: %s", hostname);
        strncpy(nodename,hostname,strlen(hostname)+1);
        free(hostname);

        /* Unpack the host_ip - 4 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        if (NULL == hostname) {
            ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("host_ip");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked host_ip: %s", hostname);
        free(hostname);

        /* BMC IP - 5 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &hostname, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("bmcip");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked bmcip: %s", hostname);
        free(hostname);

        /* BMC FW REV - 6 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("bmcfwrev");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sample_item);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked bmcfwrev: %s", sample_item);
        free(sample_item);

        /* IPMI VER - 7 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ipmiver");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sample_item);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked ipmiver: %s", sample_item);
        free(sample_item);

        /* Manufacturer ID - 8 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("manufacturer_id");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sample_item);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked MANUF-ID: %s", sample_item);
        free(sample_item);

        /* System Power State - 9 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("sys_power_state");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sample_item);
        opal_list_append(vals, &kv->super);
        free(sample_item);

        /* Device Power State - 10 */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("dev_power_state");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sample_item);
        opal_list_append(vals, &kv->super);
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "UnPacked DEV_PSTATE: %s", sample_item);
        free(sample_item);

        /* Log All non-string metrics here */
        for(int count_metrics=0;count_metrics<TOTAL_FLOAT_METRICS;count_metrics++)
        {
            /* PS1 Power Usage - 11*/
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(rc);
                return;
            }

            /* Device Power State - 10 */
            n=1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &sample_item, &n, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return;
            }
            memset(key_unit,'\0',sizeof(key_unit));
            strncpy(key_unit,metric_string[count_metrics],strlen(metric_string[count_metrics]));
            strcat(key_unit,":");
            strcat(key_unit,sample_item);

            kv = OBJ_NEW(opal_value_t);
            kv->key = strdup(key_unit);
            kv->type = OPAL_FLOAT;
            kv->data.fval = float_item;
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "UnPacked %s: %f", key_unit, float_item);
            opal_list_append(vals, &kv->super);
            free(sample_item);
        }
        /* Send the unpacked data for one Node */
        /* store it */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store(orcm_sensor_base.dbhandle, "ipmi", vals, mycleanup, NULL);
        } else {
            OPAL_LIST_RELEASE(vals);
        }
    }
}

static void generate_test_vector(opal_buffer_t *v)
{

}
void orcm_sensor_ipmi_get_system_power_state(uchar in, char* str)
{
    char in_r = in & 0x7F;
    switch(in_r) {
        case 0x0:   strncpy(str,"S0/G0",sizeof("S0/G0")); break;
        case 0x1:   strncpy(str,"S1",sizeof("S1")); break;
        case 0x2:   strncpy(str,"S2",sizeof("S2")); break;
        case 0x3:   strncpy(str,"S3",sizeof("S3")); break;
        case 0x4:   strncpy(str,"S4",sizeof("S4")); break;
        case 0x5:   strncpy(str,"S5/G2",sizeof("S5/G2")); break;
        case 0x6:   strncpy(str,"S4/S5",sizeof("S4/S5")); break;
        case 0x7:   strncpy(str,"G3",sizeof("G3")); break;
        case 0x8:   strncpy(str,"sleeping",sizeof("sleeping")); break;
        case 0x9:   strncpy(str,"G1 sleeping",sizeof("G1 sleeping")); break;
        case 0x0A:  strncpy(str,"S5 override",sizeof("S5 override")); break;
        case 0x20:  strncpy(str,"Legacy On",sizeof("Legacy On")); break;
        case 0x21:  strncpy(str,"Legacy Off",sizeof("Legacy Off")); break;
        case 0x2A:  strncpy(str,"Unknown",sizeof("Unknown")); break;
        default:    strncpy(str,"Illegal",sizeof("Illegal")); break;
    }
}
void orcm_sensor_ipmi_get_device_power_state(uchar in, char* str)
{
    char in_r = in & 0x7F;
    switch(in_r) {
        case 0x0:   strncpy(str,"D0",sizeof("D0")); break;
        case 0x1:   strncpy(str,"D1",sizeof("D1")); break;
        case 0x2:   strncpy(str,"D2",sizeof("D2")); break;
        case 0x3:   strncpy(str,"D3",sizeof("D3")); break;
        case 0x4:   strncpy(str,"Unknown",sizeof("Unknown")); break;
        default:    strncpy(str,"Illegal",sizeof("Illegal")); break;
    }
}

void orcm_sensor_ipmi_exec_call(ipmi_capsule_t *cap)
{
    char addr[16];
    int ret = 0;
    unsigned char idata[4], rdata[256];
    unsigned char ccode;
    int rlen = 256;
    char fdebug = 0;
    /* ipmi_capsule_t *cap = (ipmi_capsule_t*)(thread_param); */
    device_id_t devid;
    acpi_power_state_t pwr_state;

    char tag[17];
    unsigned char snum = 0;
    unsigned char reading[4];       /* Stores the individual sensor reading */
    double val;
    char *typestr;                  /* Stores the individual sensor unit */
    unsigned short int id = 0;
    unsigned char sdrbuf[SDR_SZ];
    unsigned char *sdrlist;
    char *error_string;

    char sys_pwr_state_str[16], dev_pwr_state_str[16];
    char test[16], test1[16];

    memset(rdata,0xff,256);
    memset(idata,0xff,4);
    if (cap->capability[BMC_REV] & cap->capability[IPMI_VER])
    {
        ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
        if(0 == ret)
        {
            ret = ipmi_cmd_mc(GET_DEVICE_ID, idata, 0, rdata, &rlen, &ccode, fdebug);
            if(0 == ret)
            {
                ipmi_close();
                memcpy(&devid.raw, rdata, sizeof(devid));
                /*opal_output(0, "%x.%xv : IPMI %x.%x : Manufacturer %02x%02x", (devid.bits.fw_rev_1&0x7F), (devid.bits.fw_rev_2&0xFF)
                *                                        , (devid.bits.ipmi_ver&0xF), (devid.bits.ipmi_ver&0xF0)
                *                                        , devid.bits.manufacturer_id[1], devid.bits.manufacturer_id[0]);
                * Copy all retrieved information in a global buffer
                */

                /*  Pack the BMC FW Rev */
                sprintf(test,"%x", devid.bits.fw_rev_1&0x7F);
                sprintf(test1,"%x", devid.bits.fw_rev_2&0xFF);
                strcat(test,".");
                strcat(test,test1);
                strncpy(cap->prop.bmc_rev, test, sizeof(test));

                /*  Pack the IPMI VER */
                sprintf(test,"%x", devid.bits.ipmi_ver&0xF);
                sprintf(test1,"%x", devid.bits.ipmi_ver&0xF0);
                strcat(test,".");
                strcat(test,test1);
                strncpy(cap->prop.ipmi_ver, test, sizeof(test));

                /*  Pack the Manufacturer ID */
                sprintf(test,"%02x", devid.bits.manufacturer_id[1]);
                sprintf(test1,"%02x", devid.bits.manufacturer_id[0]);
                strcat(test,test1);
                strncpy(cap->prop.man_id, test, sizeof(test));
            }

            else {
                //disable_ipmi = 1;
                error_string = decode_rv(ret);
                opal_output(0,"Unable to reach IPMI device for node: %s",cap->node.name );
                opal_output(0,"ipmi_cmd_mc ERROR : %s \n", error_string);
            }
        }

        else {
            error_string = decode_rv(ret);
            opal_output(0,"Set LAN OPTIONS ERROR : %s \n", error_string);
        }
    }

    if  (cap->capability[SYS_POWER_STATE] & cap->capability[DEV_POWER_STATE])
    { 
        memset(rdata,0xff,256);
        memset(idata,0xff,4);
        ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
        if(0 == ret)
        {
            ret = ipmi_cmd_mc(GET_ACPI_POWER, idata, 0, rdata, &rlen, &ccode, fdebug);
            if(0 == ret)
            {
                ipmi_close();
                memcpy(&pwr_state.raw, rdata, sizeof(pwr_state));
                orcm_sensor_ipmi_get_system_power_state(pwr_state.bits.sys_power_state, sys_pwr_state_str);
                orcm_sensor_ipmi_get_device_power_state(pwr_state.bits.dev_power_state, dev_pwr_state_str);
                /*opal_output(0, "Sys State: %s - Dev State: %s", sys_pwr_state_str, dev_pwr_state_str); */
                /* Copy all retrieved information in a global buffer */
                memcpy(cap->prop.sys_power_state,sys_pwr_state_str,sizeof(sys_pwr_state_str));
                memcpy(cap->prop.dev_power_state,dev_pwr_state_str,sizeof(dev_pwr_state_str));
            }

            else {
                error_string = decode_rv(ret);
                opal_output(0,"ipmi_cmd_mc ERROR : %s \n", error_string);
            }
        }

        else {
            error_string = decode_rv(ret);
            opal_output(0,"Set LAN OPTIONS ERROR : %s \n", error_string);
        }
    }

    /* BEGIN: Gathering SDRs */
    /* @VINFIX : NOTE!!!
    * Currently Read sensors uses the additional functionality implemented in isensor.h & ievent.h
    * These files are not part of the libipmiutil and needs to be build along with the IPMI Plugin
    * No licensing issues since they are released under FreeBSD
    */ 
    memset(rdata,0xff,256);
    memset(idata,0xff,4);
    ret = set_lan_options(cap->node.bmc_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
    if(ret)
    {
        error_string = decode_rv(ret);
        opal_output(0,"Set LAN OPTIONS ERROR : %s \n", error_string);
        return;
    }
    ret = get_sdr_cache(&sdrlist);
    while(find_sdr_next(sdrbuf,sdrlist,id) == 0) 
    {
        id = sdrbuf[0] + (sdrbuf[1] << 8); /* this SDR id */
        if (sdrbuf[3] != 0x01) continue; /* full SDR */
	    strncpy(tag,&sdrbuf[48],16);
        tag[16] = 0;
	    snum = sdrbuf[7];
        ret = GetSensorReading(snum, sdrbuf, reading);
        if (ret == 0)
        {
	        val = RawToFloat(reading[0], sdrbuf);
            typestr = get_unit_type( sdrbuf[20], sdrbuf[21], sdrbuf[22],0);
        } else {
            val = 0;
            typestr = "na";
            //opal_output(0, "%04x: get sensor %x reading ret = %d\n",id,snum,ret);
        }
        if  (cap->capability[PS1_USAGE])
        {
            if(!strncmp("PS1 Power In",tag,sizeof("PS1 Power In")))
            {
                /*  Pack the PS1 Power Usage */
                cap->prop.collection_metrics[PS1_USAGE]=val;
                strncpy(cap->prop.collection_metrics_units[PS1_USAGE],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[PS1_TEMP])
        {
            if(!strncmp("PS1 Temperature",tag,sizeof("PS1 Temperature")))
            {
                /*  Pack the PS1 Temperature */
                cap->prop.collection_metrics[PS1_TEMP]=val;
                strncpy(cap->prop.collection_metrics_units[PS1_TEMP],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[PS2_USAGE])
        {
            if(!strncmp("PS2 Power In",tag,sizeof("PS2 Power In")))
            {
                /*  Pack the PS2 Power Usage */
                cap->prop.collection_metrics[PS2_USAGE]=val;
                strncpy(cap->prop.collection_metrics_units[PS2_USAGE],typestr,strlen(typestr)+1);

            }
        }
        if  (cap->capability[PS2_TEMP])
        {
            if(!strncmp("PS2 Temperature",tag,sizeof("PS2 Temperature")))
            {
                /*  Pack the PS2 Temperature */
                cap->prop.collection_metrics[PS2_TEMP]=val;
                strncpy(cap->prop.collection_metrics_units[PS2_TEMP],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[FAN1_CPU_RPM])
        {
            if(!strncmp("Processor 1 Fan",tag,sizeof("Processor 1 Fan")))
            {
                /* Pack the Processor Fan 1 Speed */
                cap->prop.collection_metrics[FAN1_CPU_RPM]=val;
                strncpy(cap->prop.collection_metrics_units[FAN1_CPU_RPM],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[FAN2_CPU_RPM])
        {
            if(!strncmp("Processor 2 Fan",tag,sizeof("Processor 2 Fan")))
            {
                /* Pack the Processor Fan 2 Speed */
                cap->prop.collection_metrics[FAN2_CPU_RPM]=val;
                strncpy(cap->prop.collection_metrics_units[FAN2_CPU_RPM],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[FAN1_SYS_RPM])
        {
            if(!strncmp("System Fan 1",tag,sizeof("System Fan 1")))
            {
                /* Pack the System Fan 1 Speed */
                cap->prop.collection_metrics[FAN1_SYS_RPM]=val;
                strncpy(cap->prop.collection_metrics_units[FAN1_SYS_RPM],typestr,strlen(typestr)+1);
            }
        }
        if  (cap->capability[FAN2_SYS_RPM])
        {
            if(!strncmp("System Fan 2",tag,sizeof("System Fan 2")))
            {
                /* Pack the System Fan 2 Speed */
                cap->prop.collection_metrics[FAN2_SYS_RPM]=val;
                strncpy(cap->prop.collection_metrics_units[FAN2_SYS_RPM],typestr,strlen(typestr)+1);
            }
        }
	    memset(sdrbuf,0,SDR_SZ);
    }
    ipmi_close();
    /* End: gathering SDRs */
}



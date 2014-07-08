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
unsigned char disable_ipmi = 0;
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
    return;
}


static void stop(orte_jobid_t jobid)
{
    opal_output(0,"IPMI Stop");
    return;
}

static void ipmi_sample(orcm_sensor_sampler_t *sampler)
{
    ipmi_capsule_t cap;
    int rc;
    char node[TOTAL_NODES][16]={"192.168.0.102","192.168.0.100"};
    opal_buffer_t data, *bptr;
    char *ipmi;
    time_t now;
    double tdiff;
    char time_str[40];
    char *timestamp_str, *sample_str;
    struct tm *sample_time;

    if (mca_sensor_ipmi_component.test) {
        /* just send the test vector */
        bptr = &test_vector;
        opal_dss.pack(&sampler->bucket, &bptr, 1, OPAL_BUFFER);
        return;
    }

    /* prep the buffer to collect the data */
    OBJ_CONSTRUCT(&data, opal_buffer_t);
    /* pack our name */
    ipmi = strdup("ipmi");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &ipmi, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&data);
        return;
    }
    free(ipmi);

    memcpy(cap.node.user,"root", sizeof("root"));
    memcpy(cap.node.pasw,"knc@123", sizeof("knc@123"));
    cap.node.auth = IPMI_SESSION_AUTHTYPE_PASSWORD;
    cap.node.priv = IPMI_PRIV_LEVEL_ADMIN;
    cap.node.ciph = 3; /* Cipher suite No. 3 */
    if(disable_ipmi == 1)
        return;
    for(int i=0; i < TOTAL_NODES; i++)
    {
        /* Clear all memory for the ipmi_capsule */
        memset(&(cap.prop), '\0', sizeof(cap.prop));
        /* Enable/Disable the property/metric to be sampled */
        cap.capability[BMC_REV]         = 1;
        cap.capability[IPMI_VER]        = 1;
        cap.capability[SYS_POWER_STATE] = 1;
        cap.capability[DEV_POWER_STATE] = 1;
        cap.capability[PS1_USAGE]       = 1;
        cap.capability[PS1_TEMP]        = 1;
        cap.capability[FAN1_CPU_RPM]    = 1;
        cap.capability[FAN2_CPU_RPM]    = 1;
        cap.capability[FAN1_SYS_RPM]    = 1;
        cap.capability[FAN2_SYS_RPM]    = 1;

        strncpy(cap.node.node_ip, node[i], sizeof(node[i]));
        ipmi_exec_call(&cap);
        if(disable_ipmi == 1)
            return;

        /*opal_output(0," *^*^*^* %s - %s - %s - %s - %s - %s", cap.node.node_ip
        *             , cap.prop.bmc_rev, cap.prop.ipmi_ver
        *             , cap.prop.sys_power_state, cap.prop.dev_power_state
        *             , cap.prop.ps1_usage);
        */ Pack each IP address
        sample_str = strdup(cap.node.node_ip);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

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

        /*  Pack the BMC FW Rev */
        sample_str = strdup(cap.prop.bmc_rev);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /*  Pack the IPMI VER */
        sample_str = strdup(cap.prop.ipmi_ver);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /*  Pack the Manufacturer ID */
        sample_str = strdup(cap.prop.man_id);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /*  Pack the System Power State */
        sample_str = strdup(cap.prop.sys_power_state);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /*  Pack the Device Power State */
        sample_str = strdup(cap.prop.dev_power_state);
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &sample_str, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            free(sample_str);
            return;
        }
        free(sample_str);

        /*  Pack the PS1 Power Usage */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.ps1_usage, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the PS1 Temperature */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.ps1_temp, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the Processor Fan 1 speed */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.fan1_cpu_rpm, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the Processor Fan 2 speed */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.fan2_cpu_rpm, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the System Fan 1 speed */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.fan1_sys_rpm, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

        /*  Pack the System Fan 2 speed */
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&data, &cap.prop.fan2_sys_rpm, 1, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_DESTRUCT(&data);
            return;
        }

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
    /* this is currently a no-op */
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
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
    char *hostname;
    char *sampletime, *sample_item;
    float float_item;
    int rc;
    int32_t n;
    opal_list_t *vals;
    opal_value_t *kv;
    if (!log_enabled) {
        return;
    }
    if(disable_ipmi == 1)
        return;

    /* opal_output(0,"IPMI Log"); */
    /* START UNPACKING THE DATA and Store it in a opal_list_t item. */
    count_log++;
    opal_output(0,"Count Log: %d", count_log);
    for(int count = 0; count < TOTAL_NODES; count++)
    {
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
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ctime");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(sampletime);
        opal_list_append(vals, &kv->super);
        free(sampletime);

        /* hostname */
        if (NULL == hostname) {
            ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("hostname");
        kv->type = OPAL_STRING;
        kv->data.string = strdup(hostname);
        opal_list_append(vals, &kv->super);
        free(hostname);

        /* BMC FW REV */
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
        free(sample_item);

        /* IPMI VER */
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
        free(sample_item);

        /* Manufacturer ID */
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
        free(sample_item);

        /* System Power State */
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

        /* Device Power State */
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
        free(sample_item);

        /* PS1 Power Usage */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ps1_usage");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

        /* PS1 Temperature */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("ps1_temp");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

        /* CPU Fan 1 Speed */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cpu_fan_1");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

        /* CPU Fan 2 Speed */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("cpu_fan_2");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

        /* System Fan 1 Speed */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("sys_fan_1");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

        /* System Fan 2 Speed */
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(sample, &float_item, &n, OPAL_FLOAT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        kv = OBJ_NEW(opal_value_t);
        kv->key = strdup("sys_fan_2");
        kv->type = OPAL_FLOAT;
        kv->data.fval = float_item;
        opal_list_append(vals, &kv->super);

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
void get_system_power_state(uchar in, char* str)
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
void get_device_power_state(uchar in, char* str)
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

void ipmi_exec_call(ipmi_capsule_t *cap)
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

    char sys_pwr_state_str[16], dev_pwr_state_str[16];
    char test[16], test1[16];

    memset(rdata,0xff,256);
    memset(idata,0xff,4);
    if (cap->capability[BMC_REV] & cap->capability[IPMI_VER])
    {
        ret = set_lan_options(cap->node.node_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
        if(ret)
        {
            opal_output(0,"Set LAN OPTIONS RETURN CODE : %d \n", ret);
        }
        ret = ipmi_cmd_mc(GET_DEVICE_ID, idata, 0, rdata, &rlen, &ccode, fdebug);
        if(ret)
        {
            disable_ipmi = 1;
            opal_output(0,"Unable to reach IPMI device(s), plugin will be disabled!!");
            opal_output(0,"ipmi_cmd_mc RETURN CODE : %d \n", ret);
        }
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

    if  (cap->capability[SYS_POWER_STATE] & cap->capability[DEV_POWER_STATE])
    { 
        memset(rdata,0xff,256);
        memset(idata,0xff,4);
        ret = set_lan_options(cap->node.node_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
        if(ret)
        {
            opal_output(0,"Set LAN OPTIONS RETURN CODE : %d \n", ret);
        }
        ret = ipmi_cmd_mc(GET_ACPI_POWER, idata, 0, rdata, &rlen, &ccode, fdebug);
        if(ret)
        {
            opal_output(0,"ipmi_cmd_mc RETURN CODE : %d \n", ret);
        }
        ipmi_close();
        memcpy(&pwr_state.raw, rdata, sizeof(pwr_state));
        get_system_power_state(pwr_state.bits.sys_power_state, sys_pwr_state_str);
        get_device_power_state(pwr_state.bits.dev_power_state, dev_pwr_state_str);
        /*opal_output(0, "Sys State: %s - Dev State: %s", sys_pwr_state_str, dev_pwr_state_str); */
        /* Copy all retrieved information in a global buffer */
        memcpy(cap->prop.sys_power_state,sys_pwr_state_str,sizeof(sys_pwr_state_str));
        memcpy(cap->prop.dev_power_state,dev_pwr_state_str,sizeof(dev_pwr_state_str));
    }

    /* BEGIN: Gathering SDRs */
    /* @VINFIX : NOTE!!!
    * Currently Read sensors uses the additional functionality implemented in isensor.h & ievent.h
    * These files are not part of the libipmiutil and needs to be build along with the IPMI Plugin
    * No licensing issues since they are released under FreeBSD
    */ 
    memset(rdata,0xff,256);
    memset(idata,0xff,4);
    ret = set_lan_options(cap->node.node_ip, cap->node.user, cap->node.pasw, cap->node.auth, cap->node.priv, cap->node.ciph, &addr, 16);
    if(ret)
    {
        opal_output(0,"Set LAN OPTIONS RETURN CODE : %d \n", ret);
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
            /*opal_output(0, "%04x: get sensor %x reading ret = %d\n",id,snum,ret);
            * opal_output(0, "Node : %s - %04x: sensor %x %s  \treading = %.2f %s\n",cap->node.node_ip, id,snum,tag,val,typestr);
            * sprintf(test,"%.2f", val);
            * strcat(test," ");
            * strcat(test,typestr);
            * strncpy(cap->prop.ps1_usage, test, sizeof(cap->prop.man_id));
            */ 
            cap->prop.ps1_usage = val;
        }
        if  (cap->capability[PS1_USAGE])
        {
            if(!strncmp("PS1 Power In",tag,sizeof("PS1 Power In")))
            {
                /*  Pack the PS1 Power Usage */
                cap->prop.ps1_usage = val;
            }
        }
        if  (cap->capability[PS1_TEMP])
        {
            if(!strncmp("PS1 Temperature",tag,sizeof("PS1 Temperature")))
            {
                /*  Pack the PS1 Power Usage */
                cap->prop.ps1_temp = val;
            }
        }
        if  (cap->capability[FAN1_CPU_RPM])
        {
            if(!strncmp("Processor 1 Fan",tag,sizeof("Processor 1 Fan")))
            {
                /* Pack the Processor Fan 1 Speed */
                cap->prop.fan1_cpu_rpm = val;
            }
        }
        if  (cap->capability[FAN2_CPU_RPM])
        {
            if(!strncmp("Processor 2 Fan",tag,sizeof("Processor 2 Fan")))
            {
                /* Pack the Processor Fan 2 Speed */
                cap->prop.fan2_cpu_rpm = val;
            }
        }
        if  (cap->capability[FAN1_SYS_RPM])
        {
            if(!strncmp("System Fan 1",tag,sizeof("System Fan 1")))
            {
                /* Pack the System Fan 1 Speed */
                cap->prop.fan1_sys_rpm = val;
            }
        }
        if  (cap->capability[FAN2_SYS_RPM])
        {
            if(!strncmp("System Fan 2",tag,sizeof("System Fan 2")))
            {
                /* Pack the System Fan 2 Speed */
                cap->prop.fan2_sys_rpm = val;
            }
        }
	    memset(sdrbuf,0,SDR_SZ);
    }
    ipmi_close();
    /* End: gathering SDRs */
}



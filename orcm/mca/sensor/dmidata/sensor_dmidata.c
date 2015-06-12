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
#include "orte/mca/rml/rml.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_dmidata.h"
#include "sensor_dmidata_decls.h"

#include "hwloc.h"
/* declare the API functions */
static int init(void);
static void finalize(void);
static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot);
static void dmidata_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);

static void generate_test_vector(opal_buffer_t *v);


/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_dmidata_module = {
    init,
    finalize,
    NULL,
    NULL,
    NULL,
    NULL,
    dmidata_inventory_collect,
    dmidata_inventory_log
};

OBJ_CLASS_INSTANCE(dmidata_inventory_t,
                   opal_list_item_t,
                   dmidata_inv_con, dmidata_inv_des);


/* Increment the MAX_INVENTORY_KEYWORDS size with every new addition here */
/* NOTE: key for populating the lookup array
 * Each search item for a value contains 6 columns
 * The columns 1,2 and 3 are the key words for finding an inventory item
 * The column 4 contains the key word for enabling an ignore case
 * The column 5 contains a key word under which the corresponding inventory data is logged.
 * The column 6 contains an alternative TEST_VECTOR for that particular inventory item for test purpose
 */ 
static char inv_keywords[MAX_INVENTORY_KEYWORDS][MAX_INVENTORY_SUB_KEYWORDS][MAX_INVENTORY_KEYWORD_SIZE] =
    {
     {"bios","version","","","bios_version","TV_BiVer"},
     {"bios","date","","","bios_release_date","TV_BiRelDat"},
     {"bios","vendor","","","bios_vendor","TV_BiVen"},
     {"cpu","vendor","","","cpu_vendor","TV_CpVen"},
     {"cpu","family","","","cpu_family","987"},
     {"cpu","model","","number","cpu_model","TV_CpMod"},
     {"cpu","model","number","","cpu_model_number","654"},
     {"board","name","","","bb_model","TV_BbMod"},
     {"board","vendor","","","bb_vendor","TV_BbVen"},
     {"board","serial","","","bb_serial","TV_BbSer"},
     {"board","version","","","bb_version","TV_BbVer"},
     {"product","uuid","","","product_uuid","TV_PrUuid"},
     {"pci","vendor","","","pci_vendor","TV_PciVen"},
     {"pci","device","","","pci_device","TV_PciDev"},
     {"pci","slot","","","pci_device","TV_PciDev"},
     {"address","","","","MAC_address","TV_MACAddr"},
    };

enum inv_item_req
    { 
        INVENTORY_KEY, 
        INVENTORY_TEST_VECTOR 
    };  
/* Contains list of hosts that have collected and upstreamed their inventory data
 * Each link is of the type 'dmidata_inventory_t' */
opal_list_t dmidata_host_list;
hwloc_topology_t dmidata_hwloc_topology;
bool sensor_set_hwloc;
bool cpufreq_loaded = false;

static int init(void)
{
    FILE *fptr;
    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "dmidata init");
    OBJ_CONSTRUCT(&dmidata_host_list, opal_list_t);
    sensor_set_hwloc = false;
    if(NULL == opal_hwloc_topology)
    {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "opal_hwloc_topology object uninitialized;Running Init");
        if(0 == hwloc_topology_init(&dmidata_hwloc_topology)) {
            if(0 == hwloc_topology_load(dmidata_hwloc_topology)) {
                sensor_set_hwloc = true;
            } else {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unable to load hwloc data");
                return ORCM_ERROR;
            }
        } else {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Unable to initialize hwloc data");
            return ORCM_ERROR;
        }
    } else {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "hwloc data already initialized");
        dmidata_hwloc_topology = opal_hwloc_topology;
        sensor_set_hwloc = false;
    }
    /* Check if cpufreq driver is loaded to collect available frequencies */
    if(NULL != (fptr = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r"))) {
        fclose(fptr);
        cpufreq_loaded = true;
    } else {
        opal_output(0,"cpufreq module not loaded, unable to collect frequency list");
        cpufreq_loaded = false;
    }
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"dmidata finalize");
    OPAL_LIST_DESTRUCT(&dmidata_host_list);
    if(true == sensor_set_hwloc) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "Destroying sensor initialized hwloc object");
        hwloc_topology_destroy(dmidata_hwloc_topology);
    }
}

static char* check_inv_key(char *inv_key, enum inv_item_req req)
{
    int i = 0;

    while (i<MAX_INVENTORY_KEYWORDS)
    {
        if ((NULL != strcasestr(inv_key,inv_keywords[i][0])) & (NULL != strcasestr(inv_key,inv_keywords[i][1])) & (NULL != strcasestr(inv_key,inv_keywords[i][2])))
        {
            if((NULL != strcasestr(inv_key,inv_keywords[i][3])) & (strlen(inv_keywords[i][3])> 0))
            {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Discarding the inventory item %s with ignore string %s ",inv_keywords[i][3],inv_key);
            } else {
                if(req == INVENTORY_KEY)
                    return inv_keywords[i][4]; /* Returns the inventory item's key */
                else
                    return inv_keywords[i][5]; /* Returns the inventory item's test vector value */
            }
        }
        i++;
    }
    return NULL;
}
static void extract_baseboard_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    orcm_metric_value_t *mkv;
    char *inv_key;
    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_MACHINE, 0))) {
        /* there are no Sockets identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-machine", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }

    /* Pack the total MACHINE Stats present and to be copied */
    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
        {
            mkv = OBJ_NEW(orcm_metric_value_t);
            mkv->value.type = OPAL_STRING;
            mkv->value.key = strdup(inv_key);
            mkv->value.data.string = strdup(obj->infos[k].value);
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Found Inventory Item %s : %s",inv_key,obj->infos[k].value);
        }
    }
}

static void extract_cpu_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    orcm_metric_value_t *mkv;
    char *inv_key;
    unsigned int socket_count=1;
    /* SOCKET Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_SOCKET, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-socket", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }
    while(NULL != obj->next_cousin)
    {
        socket_count++;
        obj=obj->next_cousin;
    }
    mkv = OBJ_NEW(orcm_metric_value_t);
    mkv->value.type = OPAL_UINT;
    mkv->value.key = strdup("num_sockets");;
    mkv->value.data.uint = socket_count;
    opal_list_append(newhost->records, (opal_list_item_t *)mkv);

    /* Pack the total SOCKET Stats present and to be copied */
    /* @VINFIX: Collect the CPU information of each CPU in each SOCKET */
    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
        {
            mkv = OBJ_NEW(orcm_metric_value_t);
            mkv->value.key = strdup(inv_key);
            if(!strncmp(inv_key,"cpu_model_number",sizeof("cpu_model_number")) |
               !strncmp(inv_key,"cpu_family",sizeof("cpu_family")))
            {
                mkv->value.type = OPAL_INT;
                mkv->value.data.integer = strtol(obj->infos[k].value,NULL,10);
            } else {
                mkv->value.type = OPAL_STRING;
                mkv->value.data.string = strdup(obj->infos[k].value);
            }
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Found Inventory Item %s : %s",inv_key,obj->infos[k].value);
        }
    }
}

/* Extract the PCI Device with BLOCK class codes */
static void extract_blk_inventory(hwloc_obj_t obj, uint32_t pci_count, dmidata_inventory_t *newhost)
{
    uint32_t k;
    orcm_metric_value_t *mkv;
    char *inv_key;
    
    if(false == mca_sensor_dmidata_component.blk_dev) {
        return;
    }
    mkv = OBJ_NEW(orcm_metric_value_t);
    asprintf(&mkv->value.key,"pci_type_%d",pci_count);
    mkv->value.type = OPAL_STRING;
    mkv->value.data.string = strdup("BlockDevice");
    opal_list_append(newhost->records, (opal_list_item_t *)mkv);
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Found PCI Item %s : %s",mkv->value.key,mkv->value.data.string);

    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
        {
            mkv = OBJ_NEW(orcm_metric_value_t);
            asprintf(&mkv->value.key,"%s_%d",inv_key,pci_count);
            mkv->value.type = OPAL_STRING;
            mkv->value.data.string = strdup(obj->infos[k].value);
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Found PCI Item %s : %s",mkv->value.key,mkv->value.data.string);
        }
    }
}

/* Extract the PCI Device with NETWORK class codes */
static void extract_ntw_inventory(hwloc_obj_t obj, uint32_t pci_count, dmidata_inventory_t *newhost)
{
    uint32_t k;
    orcm_metric_value_t *mkv;
    char *inv_key;
    if(false == mca_sensor_dmidata_component.ntw_dev) {
        return;
    }
    mkv = OBJ_NEW(orcm_metric_value_t);
    asprintf(&mkv->value.key,"pci_type_%d",pci_count);
    mkv->value.type = OPAL_STRING;
    mkv->value.data.string = strdup("NetworkDevice");
    opal_list_append(newhost->records, (opal_list_item_t *)mkv);
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "Found PCI Item %s : %s",mkv->value.key,mkv->value.data.string);

    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
        {
            mkv = OBJ_NEW(orcm_metric_value_t);
            asprintf(&mkv->value.key,"%s_%d",inv_key,pci_count);
            mkv->value.type = OPAL_STRING;
            mkv->value.data.string = strdup(obj->infos[k].value);
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Found PCI Item %s : %s",mkv->value.key,mkv->value.data.string);
        }
    }
    if (NULL != obj->first_child) {
        if (HWLOC_OBJ_OS_DEVICE == obj->first_child->type)
        {
            obj = obj->first_child;
            for (k=0; k < obj->infos_count; k++) {
                if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
                {
                    mkv = OBJ_NEW(orcm_metric_value_t);
                    asprintf(&mkv->value.key,"%s_%d",inv_key,pci_count);
                    mkv->value.type = OPAL_STRING;
                    mkv->value.data.string = strdup(obj->infos[k].value);
                    opal_list_append(newhost->records, (opal_list_item_t *)mkv);
                    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                        "Found PCI Item %s : %s",mkv->value.key,mkv->value.data.string);
                }
            }


        }
    }

}

static void extract_pci_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t pci_count=0;
    unsigned char pci_class, pci_subclass;

    if((false == mca_sensor_dmidata_component.pci_dev)&(false == mca_sensor_dmidata_component.ntw_dev) &
        (false == mca_sensor_dmidata_component.blk_dev)) {
        return;
    }

    /* SOCKET Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_PCI_DEVICE, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-pci", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }

    while (NULL != obj) {
        pci_class = ((obj->attr->pcidev.class_id)&(0xFF00))>>8;
        pci_subclass =  ((obj->attr->pcidev.class_id)&(0xFF));
        switch (pci_class) {
            case 0x01: /*Block/Mass storage Device*/
                        if(true == mca_sensor_dmidata_component.blk_dev) { /* Check for the user defined mca parameter */
                            extract_blk_inventory(obj,pci_count,newhost);
                            pci_count++;
                        }
                        obj = obj->next_cousin;
                        continue;
            case 0x02: /*Network Device*/
                        if (true == mca_sensor_dmidata_component.ntw_dev) { /* Check for the user defined mca paramer */
                            extract_ntw_inventory(obj,pci_count,newhost);
                            pci_count++;
                        }
                        obj = obj->next_cousin;
                        continue;
            default: /*Other PCI Device, currently not required */
                        obj = obj->next_cousin;
                        continue;
        }
        obj = obj->next_cousin;
    }
}

static void extract_cpu_freq_steps(char *freq_step_list, char *hostname, dmidata_inventory_t *newhost)
{
    orcm_metric_value_t *mkv;
    int size = 0, i;
    char **freq_list_token;

    if(false == mca_sensor_dmidata_component.freq_steps) {
        return;
    }
    if(strcmp(freq_step_list,"NULL") == 0) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Frequency Steps not available");
        return;
    } else {
        freq_list_token = opal_argv_split(freq_step_list, ' ');
        size = opal_argv_count(freq_list_token)-1;
        mkv = OBJ_NEW(orcm_metric_value_t);
        asprintf(&mkv->value.key,"total_freq_steps");
        mkv->value.type = OPAL_UINT;
        mkv->value.data.uint = size;
        opal_list_append(newhost->records, (opal_list_item_t *)mkv);

        for(i = 0; i < size; i++) {
            mkv = OBJ_NEW(orcm_metric_value_t);
            asprintf(&mkv->value.key,"freq_step%d",i);
            mkv->value.type = OPAL_UINT;
            mkv->units = strdup("kHz");
            mkv->value.data.uint = strtol(freq_list_token[i],NULL,10);
            opal_list_append(newhost->records, (opal_list_item_t *)mkv);
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Found Inventory Item freq_step%d : %d",i,mkv->value.data.uint);
        }
    }
    newhost->freq_step_list = strdup(freq_step_list);
}
static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int32_t rc;
    char *comp;
    FILE *fptr;
    char *freq_list;
    int size = 0;
    if (mca_sensor_dmidata_component.test) {
        /* just send the test vector */
        generate_test_vector(inventory_snapshot);
        return;
    }
    comp = strdup("dmidata");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    free(comp);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &dmidata_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    if (cpufreq_loaded == true) {
        if(NULL != (fptr = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r"))) {
            fseek(fptr, 0, SEEK_END);
            size = ftell(fptr);
            freq_list = (char*)malloc(size);
            if (NULL == freq_list) {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Unable to allocate memory");
                fclose(fptr);
                return;
            }
            rewind(fptr);
            fgets(freq_list, size, fptr);
            fclose(fptr);
         } else{
            freq_list = strdup("NULL");
         }
    } else {
        freq_list = strdup("NULL");
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &freq_list, 1, OPAL_STRING))) {
        free(freq_list);
        ORTE_ERROR_LOG(rc);
        return;
    }
    free(freq_list);
}

static dmidata_inventory_t* found_inventory_host(char * nodename)
{
    dmidata_inventory_t *host, *nxt;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node in dmidata inventory: %s", nodename);
    OPAL_LIST_FOREACH_SAFE(host, nxt, &dmidata_host_list, dmidata_inventory_t) {
        if(!strcmp(nodename,host->nodename))
        {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "Found Node: %s", nodename);
            return host;
        }
    }
    return NULL;
}


static void dmidata_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot)
{
    hwloc_topology_t topo;
    int32_t n, rc;
    dmidata_inventory_t *newhost;
    char *freq_step_list;
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &topo, &n, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &freq_step_list, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if (NULL != (newhost = found_inventory_host(hostname)))
    {
        /* Check and Verify Node Inventory record and update db/notify user accordingly */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "dmidata HOST found!!");
        if((opal_dss.compare(topo, newhost->hwloc_topo,OPAL_HWLOC_TOPO) == OPAL_EQUAL) & 
           (strncmp(freq_step_list,newhost->freq_step_list, strlen(freq_step_list)) == 0) ) {

            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Compared values match for : hwloc; Do nothing");
        } else {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Value mismatch : hwloc; Notify User; Update List; Update Database");
            opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

            /* Delete existing host and update the host list with the freshly 
             * received inventory details*/
            OPAL_LIST_RELEASE(newhost->records);
            free(newhost->freq_step_list);
            newhost->records=OBJ_NEW(opal_list_t);

            /*Extract all required inventory items here */
            extract_baseboard_inventory(topo, hostname, newhost);
            extract_cpu_inventory(topo, hostname, newhost);
            extract_cpu_freq_steps(freq_step_list, hostname, newhost);

            /* Send the collected inventory details to the database for storage */
            if (0 <= orcm_sensor_base.dbhandle) {
                orcm_db.update_node_features(orcm_sensor_base.dbhandle,
                    newhost->nodename, newhost->records, NULL, NULL);
            }
        }
    } else { /* Node not found, Create new node and attach inventory details */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Received dmidata inventory log from a new host:%s", hostname);
        newhost = OBJ_NEW(dmidata_inventory_t);
        newhost->nodename = strdup(hostname);
        /* @VINFIX: Need to fix the bug in dss copy for OPAL_HWLOC_TOPO */
        opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

        /*Extract all required inventory items here */
        extract_baseboard_inventory(topo, hostname, newhost);
        extract_cpu_inventory(topo, hostname, newhost);
        extract_cpu_freq_steps(freq_step_list, hostname, newhost);
        extract_pci_inventory(topo, hostname, newhost);

        /* Append the new node to the existing host list */
        opal_list_append(&dmidata_host_list, &newhost->super);

        /* Send the collected inventory details to the database for storage */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.update_node_features(orcm_sensor_base.dbhandle, newhost->nodename , newhost->records, 
            NULL, NULL);
        }
    }
}

static void generate_test_vector(opal_buffer_t *v)
{
    int rc;
    char *ctmp;
    hwloc_obj_t obj;
    char *inv_key, *inv_tv, *freq_list;
    uint32_t k;

    ctmp = strdup("dmidata");
    opal_dss.pack(v, &ctmp, 1, OPAL_STRING);
    free(ctmp);

    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(dmidata_hwloc_topology, HWLOC_OBJ_MACHINE, 0))) {
        return;
    }
    /* Pack the total MACHINE Stats present and to be copied */
    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
        {
            inv_tv = check_inv_key(obj->infos[k].name, INVENTORY_TEST_VECTOR);
            if(NULL != inv_tv) {
                free(obj->infos[k].value);
                obj->infos[k].value = strdup(inv_tv);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Found Inventory Item %s : %s",inv_key, obj->infos[k].value);
            } else {
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Unable to retrieve object from test vector");
                return;
            }
        }
    }

    /* SOCKET Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(dmidata_hwloc_topology, HWLOC_OBJ_SOCKET, 0))) {
        return;
    }
    /* Set Text Vector for all cousins at the OBJ_SOCKET level */
    while(NULL != (obj->next_cousin))
    {
        obj=obj->next_cousin;
        /* Pack the total CPU Stats present and to be copied */
        for (k=0; k < obj->infos_count; k++) {
            if(NULL != (inv_tv = check_inv_key(obj->infos[k].name, INVENTORY_TEST_VECTOR)))
            {
                free(obj->infos[k].value);
                obj->infos[k].value = strdup(inv_tv);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Found Inventory Item %s : %s",obj->infos[k].name, obj->infos[k].value);
            }
        }
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(v, &dmidata_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    freq_list = strdup("2102100 2000000 3453534 \n");

    if (OPAL_SUCCESS != (rc = opal_dss.pack(v, &freq_list, 1, OPAL_STRING))) {
        free(freq_list);
        ORTE_ERROR_LOG(rc);
        return;
    }

}


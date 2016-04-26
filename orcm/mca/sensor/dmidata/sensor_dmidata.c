/*
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
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
#include "orte/mca/rml/rml.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

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

/* Removed temporarily because the current test vector requires root and uses
   hwloc.  This is not the goal of test vectors.  This will be repaired at some
   point in the future. */
#if 0
static void generate_test_vector(opal_buffer_t *v);
#endif

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
    };

static char osdev_keywords[MAX_OSDEV_INVENTORY_KEYWORDS][MAX_INVENTORY_SUB_KEYWORDS][MAX_INVENTORY_KEYWORD_SIZE] = {
     {"address","","","","MAC_address","TV_MACAddr"},
     {"linuxdeviceid","","","","linux_device_id","TV_DeviceID"},
     {"vendor","","","","vendor","TV_DeviceVen"},
     {"model","","","","model","TV_DeviceMod"},
     {"revision","","","","revision","TV_DeviceRev"},
     {"serialnumber","","","","serial_number","TV_DeviceSN"},
     {"type","","","","type","TV_DeviceType"},
};

static char misc_keywords[MAX_MISC_INVENTORY_KEYWORDS][MAX_INVENTORY_SUB_KEYWORDS][MAX_INVENTORY_KEYWORD_SIZE] = {
     {"Type","","","","type","TV_miscType"},
     {"DeviceLocation","","","","device_location","TV_DeviceLoc"},
     {"BankLocation","","","","bank_location","TV_BankLoc"},
     {"Vendor","","","","vendor","TV_Vendor"},
     {"SerialNumber","","","","serial_number","TV_SerialNum"},
     {"AssetTag","","","","asset_tag","TV_AssetTag"},
     {"PartNumber","","","","part_number","TV_PartNum"},
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


#define addStringRecord(key_str, valueStr) {      \
    mkv = OBJ_NEW(orcm_value_t);    \
    mkv->value.type = OPAL_STRING;         \
    mkv->value.key = (key_str);                  \
    mkv->value.data.string = (valueStr);        \
    opal_list_append(newhost->records, (opal_list_item_t *)mkv);        \
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output, \
      "Found %s : %s",mkv->value.key, mkv->value.data.string);         \
}

void dmidata_inv_con(dmidata_inventory_t *trk)
{
    trk->nodename = NULL;
    trk->freq_step_list = NULL;
    trk->records = OBJ_NEW(opal_list_t);
    trk->hwloc_topo = NULL;
}

void dmidata_inv_des(dmidata_inventory_t *trk)
{
    if(NULL != trk) {
        if(NULL != trk->records) {
            ORCM_RELEASE(trk->records);
        }
        if (NULL != trk->nodename) {
            SAFEFREE(trk->nodename);
        }
        if (NULL != trk->freq_step_list) {
            SAFEFREE(trk->freq_step_list);
        }
        if(NULL != trk->hwloc_topo) {
            opal_hwloc_base_free_topology(trk->hwloc_topo);
            trk->hwloc_topo = NULL;
        }
    }
}

static int init(void)
{
    FILE *fptr;
    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "dmidata init");
    OBJ_CONSTRUCT(&dmidata_host_list, opal_list_t);
    sensor_set_hwloc = false;
    if(NULL == opal_hwloc_topology) {
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
        opal_output(0,"Unable to collect frequency list");
        cpufreq_loaded = false;
    }
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"dmidata finalize");
    OBJ_DESTRUCT(&dmidata_host_list);
    if(true == sensor_set_hwloc) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "Destroying sensor initialized hwloc object");
        hwloc_topology_destroy(dmidata_hwloc_topology);
    }
}

static char* check_misc_key(char *misc_key, enum inv_item_req req)
{
    int i = 0;

    while (i<MAX_MISC_INVENTORY_KEYWORDS) {
        if (0 == strcasecmp(misc_key,misc_keywords[i][0])) {
            if(req == INVENTORY_KEY)
                return misc_keywords[i][4]; /* Returns the inventory item's key */
            else
                return misc_keywords[i][5]; /* Returns the inventory item's test vector value */
        }
        i++;
    }
    return NULL;
}

static char* check_osdev_key(char *inv_key, enum inv_item_req req)
{
    int i = 0;

    while (i<MAX_OSDEV_INVENTORY_KEYWORDS) {
        if (0 == strcasecmp(inv_key,osdev_keywords[i][0])) {
            if(req == INVENTORY_KEY)
                return osdev_keywords[i][4]; /* Returns the inventory item's key */
            else
                return osdev_keywords[i][5]; /* Returns the inventory item's test vector value */
        }
        i++;
    }
    return NULL;
}

static char* check_inv_key(char *inv_key, enum inv_item_req req)
{
    int i = 0;

    while (i<MAX_INVENTORY_KEYWORDS) {
        if ((NULL != strcasestr(inv_key,inv_keywords[i][0])) & (NULL != strcasestr(inv_key,inv_keywords[i][1])) & (NULL != strcasestr(inv_key,inv_keywords[i][2]))) {
            if((NULL != strcasestr(inv_key,inv_keywords[i][3])) & (strlen(inv_keywords[i][3])> 0)) {
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
    orcm_value_t *mkv;
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
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY))) {
            addStringRecord(strdup(inv_key), strdup(obj->infos[k].value));
        }
    }
}

static void extract_cpu_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    orcm_value_t *mkv;
    char *inv_key;
    unsigned int socket_count=1;
    /* SOCKET Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_SOCKET, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-socket", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }
    while(NULL != obj->next_cousin) {
        socket_count++;
        obj=obj->next_cousin;
    }
    mkv = OBJ_NEW(orcm_value_t);
    mkv->value.type = OPAL_UINT;
    mkv->value.key = strdup("num_sockets");;
    mkv->value.data.uint = socket_count;
    opal_list_append(newhost->records, (opal_list_item_t *)mkv);

    /* Pack the total SOCKET Stats present and to be copied */
    /* @VINFIX: Collect the CPU information of each CPU in each SOCKET */
    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY))) {
            mkv = OBJ_NEW(orcm_value_t);
            mkv->value.key = strdup(inv_key);
            if(!strncmp(inv_key,"cpu_model_number",sizeof("cpu_model_number")) |
               !strncmp(inv_key,"cpu_family",sizeof("cpu_family"))) {
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
    orcm_value_t *mkv;
    char *inv_key;
    char *key_str;

    if(false == mca_sensor_dmidata_component.blk_dev) {
        return;
    }

    asprintf(&key_str,"pci_type_%d",pci_count);
    addStringRecord(key_str, strdup("BlockDevice"));

    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY))) {
            asprintf(&key_str,"%s_%d",inv_key,pci_count);
            addStringRecord(key_str, strdup(obj->infos[k].value));
        }
    }

    if (NULL != obj->first_child) {
        if (HWLOC_OBJ_OS_DEVICE == obj->first_child->type) {
            obj = obj->first_child;
            for (k=0; k < obj->infos_count; k++) {
                if(NULL != (inv_key = check_osdev_key(obj->infos[k].name, INVENTORY_KEY))) {
                    asprintf(&key_str,"%s_%d",inv_key,pci_count);
                    addStringRecord(key_str, strdup(obj->infos[k].value));
                }
            }


        }
    }
}
/* Extract the PCI Device with NETWORK class codes */
static void extract_ntw_inventory(hwloc_obj_t obj, uint32_t pci_count, dmidata_inventory_t *newhost)
{
    uint32_t k;
    orcm_value_t *mkv;
    char *inv_key;
    char *key_str;
    if(false == mca_sensor_dmidata_component.ntw_dev) {
        return;
    }
    asprintf(&key_str,"pci_type_%d",pci_count);
    addStringRecord(key_str, strdup("NetworkDevice"));

    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY))) {
            asprintf(&key_str,"%s_%d",inv_key,pci_count);
            addStringRecord(key_str, strdup(obj->infos[k].value));
        }
    }
    if (NULL != obj->first_child) {
        if (HWLOC_OBJ_OS_DEVICE == obj->first_child->type) {
            obj = obj->first_child;
            for (k=0; k < obj->infos_count; k++) {
                if(NULL != (inv_key = check_osdev_key(obj->infos[k].name, INVENTORY_KEY))) {
                    asprintf(&key_str,"%s_%d",inv_key,pci_count);
                    addStringRecord(key_str, strdup(obj->infos[k].value));
                }
            }
        }
    }
}

static void extract_pci_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    hwloc_obj_t prev;
    uint32_t pci_count=0;
    unsigned char pci_class;

    if((false == mca_sensor_dmidata_component.pci_dev)&(false == mca_sensor_dmidata_component.ntw_dev) &
        (false == mca_sensor_dmidata_component.blk_dev)) {
        return;
    }

    /* SOCKET Level Stats*/

    prev = NULL;
    if (NULL == (obj = hwloc_get_next_pcidev(topo, prev))){
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-pci", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }

    while (NULL != obj) {
        pci_class = ((obj->attr->pcidev.class_id)&(0xFF00))>>8;
        switch (pci_class) {
            case 0x01: /*Block/Mass storage Device*/
                if(true == mca_sensor_dmidata_component.blk_dev) { /* Check for the user defined mca parameter */
                    extract_blk_inventory(obj,pci_count,newhost);
                    pci_count++;
                }
                break;
           case 0x02: /*Network Device*/
                if (true == mca_sensor_dmidata_component.ntw_dev) { /* Check for the user defined mca paramer */
                    extract_ntw_inventory(obj,pci_count,newhost);
                    pci_count++;
                }
                break;
            default: /*Other PCI Device, currently not required */
                break;
        }
        prev = obj;
        obj = hwloc_get_next_pcidev(topo, prev);
    }
}

static void extract_memory_inventory(hwloc_topology_t topo, char *hostname, dmidata_inventory_t *newhost)
{
    orcm_value_t *mkv;
    hwloc_obj_t obj;
    hwloc_obj_t memobj;
    uint32_t mem_count;
    const char *value;
    char *key_str;
    char *misc_key;
    char *c_start;
    char *c_end;
    unsigned int i, k, len;

    if (false == mca_sensor_dmidata_component.mem_dev){
        return;
    }

    /*
     * The memory objects are attached as children to root.  They are not available through
     * hwloc_*_by_type() calls.  This will be fixed in hwloc v2.  Also, memory should be
     * under the NumaNode it are closest to, but at this point in time hwloc developers
     * can't say for sure what that is.
     */

    mem_count = 0;
    obj = hwloc_get_root_obj(topo);

    for (i=0; i < obj->arity; i++){
        memobj = obj->children[i];
        if (memobj->type != HWLOC_OBJ_MISC){
          continue;
        }
        value = hwloc_obj_get_info_by_name(memobj, "Type");
        if (NULL == value || 0 != strcmp(value, "MemoryModule")) {
          continue;
        }

        mem_count++;

        for (k=0; k < memobj->infos_count; k++) {
            if(NULL != (misc_key = check_misc_key(memobj->infos[k].name, INVENTORY_KEY))) {
                asprintf(&key_str,"%s_%d",misc_key,mem_count);

                /* Some memory value fields are padded with spaces at the end. */
                c_start = memobj->infos[k].value;
                len = strlen(c_start);
                c_end = (0 < len) ? (c_start + len - 1) : c_start;
                while ((c_end != c_start) && isspace((int)(*c_end))) c_end--;
                len = c_end - c_start + 1;

                addStringRecord(key_str, strndup(c_start, len));
            }
        }
    }

    if (0 == mem_count){
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-memory", true, hostname);
        ORTE_ERROR_LOG(ORTE_ERROR);
    }
}
static void extract_cpu_freq_steps(char *freq_step_list, char *hostname, dmidata_inventory_t *newhost)
{
    orcm_value_t *mkv;
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
        mkv = OBJ_NEW(orcm_value_t);
        asprintf(&mkv->value.key,"total_freq_steps");
        mkv->value.type = OPAL_UINT;
        mkv->value.data.uint = size;
        opal_list_append(newhost->records, (opal_list_item_t *)mkv);

        for(i = 0; i < size; i++) {
            mkv = OBJ_NEW(orcm_value_t);
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
    opal_argv_free(freq_list_token);
}
static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int32_t rc;
    const char *comp = "dmidata";
    FILE *fptr;
    char *freq_list;
    int size = 0;

/* Removed temporarily because the current test vector requires root and uses
   hwloc.  This is not the goal of test vectors.  This will be repaired at some
   point in the future. */
#if 0
    if (mca_sensor_dmidata_component.test) {
        /* just send the test vector */
        generate_test_vector(inventory_snapshot);
        return;
    }
#endif

    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
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
        if(!strcmp(nodename,host->nodename)) {
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
    dmidata_inventory_t *newhost = NULL;
    char *freq_step_list = NULL;
    opal_value_t *kv = NULL;
    orcm_value_t *time_stamp = NULL;
    struct timeval current_time;

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &topo, &n, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &freq_step_list, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }

    gettimeofday(&current_time, NULL);
    time_stamp = orcm_util_load_orcm_value("ctime", &current_time, OPAL_TIMEVAL, NULL);
    if (NULL == time_stamp) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        goto cleanup;
    }

    if (NULL != (newhost = found_inventory_host(hostname))) {
        /* Check and Verify Node Inventory record and update db/notify user accordingly */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "dmidata HOST found!!");
        if((opal_dss.compare(topo, newhost->hwloc_topo,OPAL_HWLOC_TOPO) == OPAL_EQUAL) &&
           (strncmp(freq_step_list,newhost->freq_step_list, strlen(freq_step_list)) == 0) ) {

            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Compared values match for : hwloc; Do nothing");
            goto cleanup;
        } else {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                "Value mismatch : hwloc; Notify User; Update List; Update Database");
            if(NULL != newhost->hwloc_topo) {
                opal_hwloc_base_free_topology(newhost->hwloc_topo);
                newhost->hwloc_topo = NULL;
            }
            newhost->hwloc_topo = topo;
            topo = NULL;

            /* Delete existing host and update the host list with the freshly
             * received inventory details*/
            ORCM_RELEASE(newhost->records);
            SAFEFREE(newhost->freq_step_list);
            newhost->records=OBJ_NEW(opal_list_t);
        }
    } else { /* Node not found, Create new node and attach inventory details */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
            "Received dmidata inventory log from a new host:%s", hostname);
        newhost = OBJ_NEW(dmidata_inventory_t);
        newhost->nodename = strdup(hostname);
        newhost->hwloc_topo = topo;
        topo = NULL;

        /* Append the new node to the existing host list */
        opal_list_append(&dmidata_host_list, &newhost->super);

    }

    kv = orcm_util_load_opal_value("hostname", newhost->nodename, OPAL_STRING);
    if (NULL == kv) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "Unable to allocate data");
        goto cleanup;
    }
    opal_list_append(newhost->records, (opal_list_item_t*)time_stamp);
    time_stamp = NULL;
    opal_list_append(newhost->records, &kv->super);

    /*Extract all required inventory items here */
    extract_baseboard_inventory(newhost->hwloc_topo, hostname, newhost);
    extract_cpu_inventory(newhost->hwloc_topo, hostname, newhost);
    extract_cpu_freq_steps(freq_step_list, hostname, newhost);
    extract_pci_inventory(newhost->hwloc_topo, hostname, newhost);
    extract_memory_inventory(newhost->hwloc_topo, hostname, newhost);

    /* Send the collected inventory details to the database for storage */
    if (0 <= orcm_sensor_base.dbhandle) {
        orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA,
                          newhost->records, NULL, NULL, NULL);
    }
cleanup:
    SAFEFREE(freq_step_list);
    ORCM_RELEASE(time_stamp);
    if(NULL != topo) {
        opal_hwloc_base_free_topology(topo);
        topo = NULL;
    }
}

/* Removed temporarily because the current test vector requires root and uses
   hwloc.  This is not the goal of test vectors.  This will be repaired at some
   point in the future. */
#if 0
static void generate_test_vector(opal_buffer_t *v)
{
    int rc;
    const char *ctmp = "dmidata";
    hwloc_obj_t obj;
    char *inv_key, *inv_tv;
    const char *freq_list;
    uint32_t k;

    opal_dss.pack(v, &ctmp, 1, OPAL_STRING);

    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(dmidata_hwloc_topology, HWLOC_OBJ_MACHINE, 0))) {
        return;
    }
    /* Pack the total MACHINE Stats present and to be copied */
    for (k=0; k < obj->infos_count; k++) {
        if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY))) {
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
    while(NULL != (obj->next_cousin)) {
        obj=obj->next_cousin;
        /* Pack the total CPU Stats present and to be copied */
        for (k=0; k < obj->infos_count; k++) {
            if(NULL != (inv_tv = check_inv_key(obj->infos[k].name, INVENTORY_TEST_VECTOR))) {
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

    freq_list = "2102100 2000000 3453534\n";

    if (OPAL_SUCCESS != (rc = opal_dss.pack(v, &freq_list, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

}
#endif

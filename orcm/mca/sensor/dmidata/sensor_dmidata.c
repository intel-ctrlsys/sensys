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

static int init(void)
{
    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        ">>>>>>>>> dmidata init");
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
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"dmidata Finalize <<<<<<<<<");
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
                    return inv_keywords[i][4];
                else
                    return inv_keywords[i][5];
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
            if(!strncmp(inv_key,"cpu_model_number",sizeof("cpu_model_number")) | !strncmp(inv_key,"cpu_family",sizeof("cpu_family")))
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

static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int32_t rc;
    char *comp;
    
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

    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &topo, &n, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if (NULL != (newhost = found_inventory_host(hostname)))
    {
        /* Check and Verify Node Inventory record and update db/notify user accordingly */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "dmidata HOST found!! Update node with inventory details");
        if(opal_dss.compare(topo, newhost->hwloc_topo,OPAL_HWLOC_TOPO) == OPAL_EQUAL) {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"Compared values match for : hwloc; Do nothing");
        }
        else {
            /*@VINFIX: Due to a bug in the opal_dss.copy for OPAL_HWLOC_TOPO data type, this else block will always get
             * hit and the data will always get copied for now
             */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"Compared values don't match for: hwloc; Notify User; Update List; Update Database");
            opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

            /*Delete existing host and update the host list with the freshly received inventory details*/
            OPAL_LIST_RELEASE(newhost->records);
            newhost->records=OBJ_NEW(opal_list_t);

            /*Extract all required inventory items here */
            extract_baseboard_inventory(topo, hostname, newhost);
            extract_cpu_inventory(topo, hostname, newhost);

            /* Send the collected inventory details to the database for storage */
            if (0 <= orcm_sensor_base.dbhandle) {
                orcm_db.update_node_features(orcm_sensor_base.dbhandle, newhost->nodename , newhost->records, NULL, NULL);
            }
        }
    } else { /* Node not found, Create new node and attach inventory details */
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"Received dmidata inventory log from a new host:%s", hostname);
        newhost = OBJ_NEW(dmidata_inventory_t);
        newhost->nodename = strdup(hostname);
        /* @VINFIX: Need to fix the bug in dss copy for OPAL_HWLOC_TOPO */
        opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

        /*Extract all required inventory items here */
        extract_baseboard_inventory(topo, hostname, newhost);
        extract_cpu_inventory(topo, hostname, newhost);

        /* Append the new node to the existing host list */
        opal_list_append(&dmidata_host_list, &newhost->super);

        /* Send the collected inventory details to the database for storage */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.update_node_features(orcm_sensor_base.dbhandle, newhost->nodename , newhost->records, NULL, NULL);
        }
    }
}

static void generate_test_vector(opal_buffer_t *v)
{
    int rc;
    char *ctmp;
    hwloc_obj_t obj;
    char *inv_key, *inv_tv;
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
            obj->infos[k].value = inv_tv;
            opal_output(0,"Found Inventory Item %s : %s",inv_key, obj->infos[k].value);
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
            if(NULL != (inv_key = check_inv_key(obj->infos[k].name, INVENTORY_KEY)))
            {
                inv_tv = check_inv_key(obj->infos[k].name, INVENTORY_TEST_VECTOR);
                obj->infos[k].value = inv_tv;
                opal_output(0,"Found Inventory Item %s : %s",inv_key, obj->infos[k].value);
            }
        }
    }

    if (OPAL_SUCCESS != (rc = opal_dss.pack(v, &dmidata_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

}


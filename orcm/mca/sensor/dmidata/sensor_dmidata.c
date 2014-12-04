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

#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_dmidata.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void start(orte_jobid_t job);
static void stop(orte_jobid_t job);
static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot);
static void dmidata_inventory_log(char *hostname, opal_buffer_t *inventory_snapshot);
/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_dmidata_module = {
    init,
    finalize,
    start,
    stop,
    NULL,
    NULL,
    dmidata_inventory_collect,
    dmidata_inventory_log
};

typedef struct {
    opal_list_item_t super;
    char *nodename;
    unsigned long hashId; /* A hash value summing up the inventory record for each node, for quick comparision */
    opal_list_t *records; /* An hwloc topology container followed by a list of inventory items */
} hwloc_inventory_t;

static void inv_con(hwloc_inventory_t *trk)
{
    trk->records = OBJ_NEW(opal_list_t);
}
static void inv_des(hwloc_inventory_t *trk)
{
    if(trk != NULL) {
        if(trk != NULL) {
            OPAL_LIST_RELEASE(trk->records);
        }
    }
}
OBJ_CLASS_INSTANCE(hwloc_inventory_t,
                   opal_list_item_t,
                   inv_con, inv_des);

/* Contains list of hosts that have collected and upstreamed their inventory data
 * Each link is of the type 'hwloc_inventory_t' */
opal_list_t hwloc_host_list;

static int init(void)
{
    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */
    opal_output(0,"DMIDATA init");
    OBJ_CONSTRUCT(&hwloc_host_list, opal_list_t);
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    //orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_INVENTORY);
    OPAL_LIST_DESTRUCT(&hwloc_host_list);
}

/*
 * Start monitoring of local temps
 */
static void start(orte_jobid_t jobid)
{
    return;
}

static void stop(orte_jobid_t jobid)
{
    return;
}

static char* check_inv_key(char *inv_key)
{
    int i = 0;

    while (i<MAX_INVENTORY_KEYWORDS)
    {
        if ((NULL != strcasestr(inv_key,inv_keywords[i][0])) & (NULL != strcasestr(inv_key,inv_keywords[i][1])) & (NULL != strcasestr(inv_key,inv_keywords[i][2])))
        {
            if((NULL != strcasestr(inv_key,inv_keywords[i][3])) & (strlen(inv_keywords[i][3])> 0))
            {
                opal_output(0,"Discarding the inventory item %s with ignore string %s ",inv_keywords[i][3],inv_key);
            } else {
                //opal_output(0, "Found a matching inventory keyword - %s with %s %s %s without %s - Assigning %s ",
                //                    inv_key, inv_keywords[i][0], inv_keywords[i][1], inv_keywords[i][2], inv_keywords[i][3], inv_keywords[i][4]);
                return inv_keywords[i][4];
            }
        }
        i++;
    }
    return NULL;
}
static void collect_baseboard_inventory(hwloc_topology_t topo, hwloc_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    opal_value_t *kv;
    char *inv_key;

    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_MACHINE, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-machine", true);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }
    if (NULL != obj) {
        /* Pack the total MACHINE Stats present and to be copied */
        for (k=0; k < obj->infos_count; k++) {
            if(NULL != (inv_key = check_inv_key(obj->infos[k].name)))
            {
                kv = OBJ_NEW(opal_value_t);
                kv->type = OPAL_STRING;
                kv->key = strdup(inv_key);
                kv->data.string = strdup(obj->infos[k].value);
                opal_list_append(newhost->records, &kv->super);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Found Inventory Item %s : %s",inv_key,obj->infos[k].value);
            }
        }
    } else {
        opal_output(0,"Unable to initialize hwloc object");
    }
}

static void collect_cpu_inventory(hwloc_topology_t topo, hwloc_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    opal_value_t *kv;
    char *inv_key;

    /* MACHINE Level Stats*/
    if (NULL == (obj = hwloc_get_obj_by_type(topo, HWLOC_OBJ_SOCKET, 0))) {
        /* there are no objects identified for this machine (Weird!) */
        orte_show_help("help-orcm-sensor-dmidata.txt", "no-machine", true);
        ORTE_ERROR_LOG(ORTE_ERROR);
        return;
    }
    if (NULL != obj) {
        /* Pack the total MACHINE Stats present and to be copied */
        for (k=0; k < obj->infos_count; k++) {
            if(NULL != (inv_key = check_inv_key(obj->infos[k].name)))
            {
                kv = OBJ_NEW(opal_value_t);
                kv->type = OPAL_STRING;
                kv->key = strdup(inv_key);
                kv->data.string = strdup(obj->infos[k].value);
                opal_list_append(newhost->records, &kv->super);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Found Inventory Item %s : %s",inv_key,obj->infos[k].value);
            }
        }
    } else {
        opal_output(0,"Unable to initialize hwloc object");
    }

}

static void dmidata_inventory_collect(opal_buffer_t *inventory_snapshot)
{
    int32_t rc;
    char *comp = strdup("dmidata");
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &comp, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    free(comp);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &opal_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
}

static hwloc_inventory_t* found_inventory_host(char * nodename)
{
    hwloc_inventory_t *host, *nxt;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node in hwloc inventory: %s", nodename);
    OPAL_LIST_FOREACH_SAFE(host, nxt, &hwloc_host_list, hwloc_inventory_t) {
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
    hwloc_inventory_t *newhost;
    /*TEMP DELETE THIS LINE */
    opal_value_t *item, *nxt;
    /************************/
    
    if (found_inventory_host(hostname) != NULL)
    {
        /* Check and Verify Node Inventory record and update db/notify user accordingly */
        opal_output(0, "HWLOC HOST found!! Update node with inventory details");
    } else { /* Node not found, Create new node and attach inventory details */
        opal_output(0,"Received hwloc inventory log from a new host:%s", hostname);
        newhost = OBJ_NEW(hwloc_inventory_t);
    }
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(inventory_snapshot, &topo, &n, OPAL_HWLOC_TOPO))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    //opal_output(0,"Received string : %s",temp);
    collect_baseboard_inventory(topo, newhost);
    collect_cpu_inventory(topo, newhost);

    /* Append the new node to the existing host list */

    
    /* Send the collected inventory details to the database for storage */
    opal_output(0,"Inventory Details for node: %s",hostname);
    OPAL_LIST_FOREACH_SAFE(item, nxt, newhost->records, opal_value_t) {
        if(item->type == OPAL_STRING)
        {
            opal_output(0,"%s\t:%s",item->key,item->data.string);
        }
    }

}




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
#include "hwloc.h"
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
    hwloc_topology_t hwloc_topo;
    opal_list_t *records; /* An hwloc topology container followed by a list of inventory items */
} dmidata_inventory_t;

static void inv_con(dmidata_inventory_t *trk)
{
    trk->records = OBJ_NEW(opal_list_t);
}
static void inv_des(dmidata_inventory_t *trk)
{
    if(trk != NULL) {
        if(trk != NULL) {
            OPAL_LIST_RELEASE(trk->records);
        }
    }
    free(trk->nodename);
}
OBJ_CLASS_INSTANCE(dmidata_inventory_t,
                   opal_list_item_t,
                   inv_con, inv_des);

/* Contains list of hosts that have collected and upstreamed their inventory data
 * Each link is of the type 'dmidata_inventory_t' */
opal_list_t dmidata_host_list;

static int init(void)
{
    /* Initialize All available resource that are present in the current system
     * that need to be scanned by the plugin */
    opal_output(0,"DMIDATA init");
    OBJ_CONSTRUCT(&dmidata_host_list, opal_list_t);
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    opal_output(0,"dmidata Finalize <<<<<<<<<");
    //orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_INVENTORY);
    OPAL_LIST_DESTRUCT(&dmidata_host_list);
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
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Discarding the inventory item %s with ignore string %s ",inv_keywords[i][3],inv_key);
            } else {
                return inv_keywords[i][4];
            }
        }
        i++;
    }
    return NULL;
}
static void collect_baseboard_inventory(hwloc_topology_t topo, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    orcm_metric_value_t *mkv;
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
                mkv = OBJ_NEW(orcm_metric_value_t);
                mkv->value.type = OPAL_STRING;
                mkv->value.key = strdup(inv_key);
                mkv->value.data.string = strdup(obj->infos[k].value);
                opal_list_append(newhost->records, (opal_list_item_t *)mkv);
                opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                    "Found Inventory Item %s : %s",inv_key,obj->infos[k].value);
            }
        }
    } else {
        opal_output(0,"Unable to initialize hwloc object");
    }
}

static void collect_cpu_inventory(hwloc_topology_t topo, dmidata_inventory_t *newhost)
{
    hwloc_obj_t obj;
    uint32_t k;
    orcm_metric_value_t *mkv;
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
                mkv = OBJ_NEW(orcm_metric_value_t);
                mkv->value.type = OPAL_STRING;
                mkv->value.key = strdup(inv_key);
                mkv->value.data.string = strdup(obj->infos[k].value);
                opal_list_append(newhost->records, (opal_list_item_t *)mkv);
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

static dmidata_inventory_t* found_inventory_host(char * nodename)
{
    dmidata_inventory_t *host, *nxt;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
        "Finding Node in dmidata inventory: %s", nodename);
    OPAL_LIST_FOREACH_SAFE(host, nxt, &dmidata_host_list, dmidata_inventory_t) {
        if(!strcmp(nodename,host->nodename))
        {
            opal_output(0,"Found node: %s",nodename);
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
        opal_output(0, "dmidata HOST found!! Update node with inventory details");
        if(opal_dss.compare(topo, newhost->hwloc_topo,OPAL_HWLOC_TOPO) == OPAL_EQUAL) {
            opal_output(0,"Compared values match for : hwloc; Do nothing");
        }
        else {
            /*@VINFIX: Due to a bug in the opal_dss.copy for OPAL_HWLOC_TOPO data type, this else block will always get
             * hit and the data will always get copied for now
             */
            opal_output(0,"Compared values don't match for: hwloc; Notify User; Update List; Update Database");
            opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

            /*Delete existing host and update the host list with the freshly received inventory details*/
            OPAL_LIST_RELEASE(newhost->records);
            newhost->records=OBJ_NEW(opal_list_t);

            /*Extract all required inventory items here */
            collect_baseboard_inventory(topo, newhost);
            collect_cpu_inventory(topo, newhost);

            /* Send the collected inventory details to the database for storage */
            if (0 <= orcm_sensor_base.dbhandle) {
                orcm_db.update_node_features(orcm_sensor_base.dbhandle, newhost->nodename , newhost->records, NULL, NULL);
            }
        }
    } else { /* Node not found, Create new node and attach inventory details */
        opal_output(0,"Received dmidata inventory log from a new host:%s", hostname);
        newhost = OBJ_NEW(dmidata_inventory_t);
        newhost->nodename = strdup(hostname);
        /* @VINFIX: Need to fix the bug in dss copy for OPAL_HWLOC_TOPO */
        opal_dss.copy((void**)&newhost->hwloc_topo,topo,OPAL_HWLOC_TOPO);

        /*Extract all required inventory items here */
        collect_baseboard_inventory(topo, newhost);
        collect_cpu_inventory(topo, newhost);

        /* Append the new node to the existing host list */
        opal_list_append(&dmidata_host_list, &newhost->super);

        /* Send the collected inventory details to the database for storage */
        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.update_node_features(orcm_sensor_base.dbhandle, newhost->nodename , newhost->records, NULL, NULL);
        }
    }
}




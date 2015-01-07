/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * IPMI resource manager sensor 
 */
#ifndef ORCM_SENSOR_DMIDATA_DECLS_H
#define ORCM_SENSOR_DMIDATA_DECLS_H

#define MAX_INVENTORY_KEYWORDS      12
#define MAX_INVENTORY_SUB_KEYWORDS  5  /* 3 for search, 1 for ignore and 1 for naming the inventory item */
#define MAX_INVENTORY_KEYWORD_SIZE  30

typedef struct {
    opal_list_item_t super;
    char *nodename;
    unsigned long hashId; /* A hash value summing up the inventory record for each node, for quick comparision */
    hwloc_topology_t hwloc_topo;
    opal_list_t *records; /* An hwloc topology container followed by a list of inventory items */
} dmidata_inventory_t;

static void dmidata_inv_con(dmidata_inventory_t *trk)
{
    trk->records = OBJ_NEW(opal_list_t);
}
static void dmidata_inv_des(dmidata_inventory_t *trk)
{
    if(trk != NULL) {
        if(trk->records != NULL) {
            OPAL_LIST_RELEASE(trk->records);
        }
        if (trk->nodename != NULL) {
            free(trk->nodename);
        }
    }
}

#endif

/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_CFGI_TYPES_H
#define MCA_CFGI_TYPES_H

/*
 * includes
 */
#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/class/opal_list.h"
#include "opal/dss/dss_types.h"

#include "orte/types.h"

#include "orcm/mca/rm/rm_types.h"
#include "orcm/mca/scd/scd_types.h"

/*
 * Global functions for MCA overall collective open and close
 */
BEGIN_C_DECLS

typedef struct {
    opal_list_item_t super;
    char *name;
    char **value;
    opal_list_t subvals;
} orcm_cfgi_xml_parser_t;
OBJ_CLASS_DECLARATION(orcm_cfgi_xml_parser_t);

typedef struct {
    opal_object_t super;
    char **mca_params;
    char **env;
    char *port;
    bool aggregator;
} orcm_config_t;
OBJ_CLASS_DECLARATION(orcm_config_t);

/* predeclare an object to avoid
 * circular references
 */
struct orcm_rack_t;

typedef struct {
    opal_list_item_t super;
    char *name;
    struct orcm_rack_t *rack;
    orte_process_name_t daemon;
    orcm_config_t config;
    orcm_node_state_t state;         //writable *only* by rm after init
    orcm_scd_node_state_t scd_state; //writable *only* by scd after init
} orcm_node_t;
OBJ_CLASS_DECLARATION(orcm_node_t);

typedef struct {
    opal_list_item_t super;
    char *name;
    orcm_node_t controller;
    opal_list_t rows;
} orcm_cluster_t;
OBJ_CLASS_DECLARATION(orcm_cluster_t);

typedef struct {
    opal_list_item_t super;
    char *name;
    orcm_cluster_t *cluster;
    orcm_node_t controller;
    opal_list_t racks;
} orcm_row_t;
OBJ_CLASS_DECLARATION(orcm_row_t);

typedef struct {
    opal_list_item_t super;
    char *name;
    orcm_row_t *row;
    orcm_node_t controller;
    opal_list_t nodes;
} orcm_rack_t;
OBJ_CLASS_DECLARATION(orcm_rack_t);

typedef struct {
    opal_list_item_t super;
    orcm_node_t controller;
    char **queues;
} orcm_scheduler_t;
OBJ_CLASS_DECLARATION(orcm_scheduler_t);

END_C_DECLS
#endif

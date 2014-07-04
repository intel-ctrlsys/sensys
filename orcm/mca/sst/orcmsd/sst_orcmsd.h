/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef SST_ORCMSD_H
#define SST_ORCMSD_H

#include "orcm/mca/sst/sst.h"

typedef struct {
    orcm_sst_base_component_t super;
    char *node_regex;
    char *base_jobid;
    char *base_vpid;
} orcm_sst_orcmsd_component_t;

ORCM_DECLSPEC extern orcm_sst_orcmsd_component_t mca_sst_orcmsd_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_orcmsd_module;

#endif /* SST_ORCMSD_H */

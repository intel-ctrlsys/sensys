/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef SST_ORCMD_H
#define SST_ORCMD_H

#include "orcm/mca/sst/sst.h"

typedef struct {
    orcm_sst_base_component_t super;
    bool scheduler_reqd;
} orcm_sst_orcmd_component_t;

ORCM_DECLSPEC extern orcm_sst_orcmd_component_t mca_sst_orcmd_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_orcmd_module;

#endif /* SST_ORCMD_H */

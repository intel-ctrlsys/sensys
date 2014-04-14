/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef SST_ORCMCTRLD_H
#define SST_ORCMCTRLD_H

#include "orcm/mca/sst/sst.h"

typedef struct {
    orcm_sst_base_component_t super;
    bool kill_dvm;
} orcm_sst_orcmctrld_component_t;

ORCM_DECLSPEC extern orcm_sst_orcmctrld_component_t mca_orcm_sst_orcmctrld_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_orcmctrld_module;

#endif /* SST_ORCMCTRLD_H */

/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef SST_EMULATOR_H
#define SST_EMULATOR_H

#include "orcm/mca/sst/sst.h"

typedef struct {
    orcm_sst_base_component_t super;
    char *target;
} orcm_sst_emulator_component_t;

ORCM_DECLSPEC extern orcm_sst_emulator_component_t mca_sst_emulator_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_emulator_module;

#endif /* SST_EMULATOR_H */

/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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
 */

#ifndef MCA_scd_mfile_EXPORT_H
#define MCA_scd_mfile_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/scd/scd.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

typedef struct {
    orcm_scd_base_component_t super;
    char *moab_interface_dir;
} mca_scd_mfile_component_t;

ORCM_MODULE_DECLSPEC extern mca_scd_mfile_component_t mca_scd_mfile_component;

ORCM_DECLSPEC extern orcm_scd_base_module_t orcm_scd_mfile_module;

END_C_DECLS

#endif /* MCA_scd_mfile_EXPORT_H */

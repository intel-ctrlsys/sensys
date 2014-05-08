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

#ifndef MCA_rm_basic_EXPORT_H
#define MCA_rm_basic_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/rm/rm.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_rm_base_component_t mca_rm_basic_component;

ORCM_DECLSPEC extern orcm_rm_base_module_t orcm_rm_basic_module;

END_C_DECLS

#endif /* MCA_rm_basic_EXPORT_H */

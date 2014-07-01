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

#ifndef MCA_diag_memtest_EXPORT_H
#define MCA_diag_memtest_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/diag/diag.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_diag_base_component_t mca_diag_memtest_component;

ORCM_DECLSPEC extern orcm_diag_base_module_t orcm_diag_memtest_module;

END_C_DECLS

#endif /* MCA_diag_memtest_EXPORT_H */

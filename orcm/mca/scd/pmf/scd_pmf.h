/*
 * Copyright (c) 2014-2015 Intel Corporation. All rights reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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

#ifndef MCA_scd_pmf_EXPORT_H
#define MCA_scd_pmf_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/scd/scd.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_scd_base_component_t mca_scd_pmf_component;

ORCM_DECLSPEC extern orcm_scd_base_module_t orcm_scd_pmf_module;

END_C_DECLS

#endif /* MCA_scd_pmf_EXPORT_H */

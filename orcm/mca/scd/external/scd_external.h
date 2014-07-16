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

#ifndef MCA_scd_external_EXPORT_H
#define MCA_scd_external_EXPORT_H

#include "orcm_config.h"

#include "orcm/mca/scd/scd.h"

BEGIN_C_DECLS

/*
 * Local Component structures
 */

ORCM_MODULE_DECLSPEC extern orcm_scd_base_component_t mca_scd_external_component;

ORCM_DECLSPEC extern orcm_scd_base_module_t orcm_scd_external_module;

ORCM_DECLSPEC int external_launch(orcm_session_t *session); 
ORCM_DECLSPEC int external_cancel(orcm_alloc_id_t sessionid); 

END_C_DECLS

#endif /* MCA_scd_external_EXPORT_H */

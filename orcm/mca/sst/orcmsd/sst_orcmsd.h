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

/*
 * stdout/stderr buffering control parameter
 */

ORCM_DECLSPEC char *orcm_node_regex;
ORCM_DECLSPEC char *orcm_base_jobid;
ORCM_DECLSPEC char *orcm_base_vpid;
ORCM_DECLSPEC extern orcm_sst_base_component_t mca_orcm_sst_orcmsd_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_orcmsd_module;

#endif /* SST_ORCMSD_H */

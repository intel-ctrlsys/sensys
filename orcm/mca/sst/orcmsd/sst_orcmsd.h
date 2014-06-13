/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef SST_OJOBD_H
#define SST_OJOBD_H

#include "orcm/mca/sst/sst.h"

/*
 * stdout/stderr buffering control parameter
 */
ORTE_DECLSPEC extern int orcm_stepd_base_num_procs;
ORTE_DECLSPEC extern char *orcm_stepd_base_jobid;
ORTE_DECLSPEC extern char *orcm_stepd_base_vpid;

ORCM_DECLSPEC extern orcm_sst_base_component_t mca_orcm_sst_orcmsd_component;
ORCM_DECLSPEC extern orcm_sst_base_module_t orcm_sst_orcmsd_module;

#endif /* SST_OJOBD_H */

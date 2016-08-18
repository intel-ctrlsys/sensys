/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2011-2012 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Oak Ridge National Labs.  All rights reserved.
 * Copyright (c) 2013      Los Alamos National Security, LLC.  All rights reserved.
 * Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_ESS_BASE_H
#define MCA_ESS_BASE_H

#include "orte_config.h"
#include "orte/types.h"

#include "orte/mca/mca.h"
#include "opal/dss/dss_types.h"

#include "orte/mca/ess/ess.h"

BEGIN_C_DECLS

/*
 * MCA Framework
 */
ORTE_DECLSPEC extern mca_base_framework_t orte_ess_base_framework;
/**
 * Select a ess module
 */
ORTE_DECLSPEC int orte_ess_base_select(void);

/*
 * stdout/stderr buffering control parameter
 */
ORTE_DECLSPEC extern int orte_ess_base_std_buffering;

ORTE_DECLSPEC extern int orte_ess_base_num_procs;
ORTE_DECLSPEC extern char *orte_ess_base_jobid;
ORTE_DECLSPEC extern char *orte_ess_base_vpid;

/*
 * Internal helper functions used by components
 */
ORTE_DECLSPEC int orte_ess_base_std_prolog(void);

ORTE_DECLSPEC int orte_ess_base_tool_setup(void);
ORTE_DECLSPEC int orte_ess_base_tool_finalize(void);


END_C_DECLS

#endif

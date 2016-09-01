/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2011-2015 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 * The Process Lifecycle Management (PLM) subsystem serves as the central
 * switchyard for all process management activities, including
 * resource allocation, process mapping, process launch, and process
 * monitoring.
 */

#ifndef ORTE_PLM_H
#define ORTE_PLM_H

/*
 * includes
 */

#include "orte_config.h"
#include "orte/types.h"

#include "orte/mca/mca.h"
#include "opal/dss/dss_types.h"
#include "opal/class/opal_pointer_array.h"

#include "orte/runtime/orte_globals.h"
#include "plm_types.h"

BEGIN_C_DECLS

/**
 * Macro for use in modules that are of type plm
 */
#define ORTE_PLM_BASE_VERSION_2_0_0 \
    ORTE_MCA_BASE_VERSION_2_1_0("plm", 2, 0, 0)

END_C_DECLS

#endif

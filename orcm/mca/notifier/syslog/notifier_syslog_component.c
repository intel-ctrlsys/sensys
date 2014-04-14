/* -*- C -*-
 *
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
*/

/*
 * includes
 */
#include "orcm_config.h"
#include "orcm/constants.h"

#include "notifier_syslog.h"


static int orcm_notifier_syslog_component_query(mca_base_module_t **module, 
                                                int *priority);

/*
 * Struct of function pointers that need to be initialized
 */
orcm_notifier_base_component_t mca_notifier_syslog_component = {
    {
        ORCM_NOTIFIER_BASE_VERSION_1_0_0,
        
        "syslog", /* MCA module name */
        ORCM_MAJOR_VERSION,  /* MCA module major version */
        ORCM_MINOR_VERSION,  /* MCA module minor version */
        ORCM_RELEASE_VERSION,  /* MCA module release version */
        NULL,
        NULL,
        orcm_notifier_syslog_component_query /* module query */
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};

static int orcm_notifier_syslog_component_query(mca_base_module_t **module, 
                                                int *priority)
{
    *priority = 1;
    *module = (mca_base_module_t *)&orcm_notifier_syslog_module;
    return ORCM_SUCCESS;    
}

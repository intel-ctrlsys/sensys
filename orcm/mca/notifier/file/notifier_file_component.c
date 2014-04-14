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
 * Copyright (c) 2009      Bull SAS.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 */

/*
 * includes
 */
#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "opal/mca/base/mca_base_var.h"

#include "orte/util/show_help.h"

#include "notifier_file.h"


static int orcm_notifier_file_register(void);
static int orcm_notifier_file_component_query(mca_base_module_t **, int *);
static int orcm_notifier_file_close(void);

/*
 * Struct of function pointers that need to be initialized
 */
orcm_notifier_file_component_t mca_notifier_file_component = {
    {
        {
            ORCM_NOTIFIER_BASE_VERSION_1_0_0,

            "file", /* MCA module name */
            ORCM_MAJOR_VERSION,  /* MCA module major version */
            ORCM_MINOR_VERSION,  /* MCA module minor version */
            ORCM_RELEASE_VERSION,  /* MCA module release version */
            NULL,
            orcm_notifier_file_close,
            orcm_notifier_file_component_query,
            orcm_notifier_file_register,
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    },
};

static int orcm_notifier_file_register(void)
{
    mca_notifier_file_component.fname = strdup("wdc");
    (void) mca_base_component_var_register(&mca_notifier_file_component.super.base_version, "name",
                                           "File name the traces should be redirected to",
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_file_component.fname);
    
    mca_notifier_file_component.priority = 10;
    (void) mca_base_component_var_register(&mca_notifier_file_component.super.base_version, "name",
                                           "Priority of the file notifier component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_file_component.priority);

    return ORCM_SUCCESS;
}

static int orcm_notifier_file_close(void)
{
    return ORCM_SUCCESS;
}

static int orcm_notifier_file_component_query(mca_base_module_t **module, 
                                              int *priority)
{
    *priority = mca_notifier_file_component.priority;
    *module = (mca_base_module_t *)&orcm_notifier_file_module;

    if (NULL == mca_notifier_file_component.fname ||
        !strlen(mca_notifier_file_component.fname)) {
        orte_show_help("help-orcm-notifier-file.txt",
                       "file name not specified", true);
        return ORCM_ERR_BAD_PARAM;
    }

    return ORCM_SUCCESS;    
}


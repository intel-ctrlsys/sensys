/* -*- C -*-
 *
 * Copyright (c) 2004-2009 The Trustees of Indiana University and Indiana
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
/** @file:
 *
 * This component proxies notification events to the Fault Tolerant
 * Backplane (See http://www.mcs.anl.gov/research/cifts/).
 * The ORTE notifier severity is translated to the corresponding
 * FTB severity before the event is published to the FTB.
 */

/*
 * includes
 */
#include "orcm_config.h"
#include "orcm/constants.h"

#include <string.h>

#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "opal/mca/base/mca_base_var.h"
#include "notifier_ftb.h"

static int orcm_notifier_ftb_close(void);
static int orcm_notifier_ftb_component_query(mca_base_module_t **module, int *priority);
static int orcm_notifier_ftb_register(void);

/*
 * Struct of function pointers that need to be initialized
 */
orcm_notifier_ftb_component_t mca_notifier_ftb_component = {
    {
        {
            ORCM_NOTIFIER_BASE_VERSION_1_0_0,
        
            "ftb", /* MCA module name */
            ORCM_MAJOR_VERSION,  /* MCA module major version */
            ORCM_MINOR_VERSION,  /* MCA module minor version */
            ORCM_RELEASE_VERSION,  /* MCA module release version */

            NULL,
            orcm_notifier_ftb_close, /* module close */
            orcm_notifier_ftb_component_query, /* module query */
            orcm_notifier_ftb_register, /* module register */
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    },
};

static int orcm_notifier_ftb_close(void)
{

    if (NULL != mca_notifier_ftb_component.subscription_style) {
        free(mca_notifier_ftb_component.subscription_style);
    }

    return ORCM_SUCCESS;
}

static int orcm_notifier_ftb_component_query(mca_base_module_t **module, 
                                             int *priority)
{
    int ret;
    *priority = 0;
    *module = NULL;

    /* Fill the FTB client information structure */
    memset(&ftb_client_info, 0, sizeof(ftb_client_info));
    strcpy(ftb_client_info.event_space, "ftb.mpi.openmpi");

    /* We represent each client with a client name of the form
       openmpi/<hostname>/<PID> as a unique identifier in the
       FTB client namespace */
    sprintf(ftb_client_info.client_name, "ompi%u", orte_process_info.pid);

    sprintf(ftb_client_info.client_jobid, "%u", ORTE_PROC_MY_NAME->jobid);

    strncpy(ftb_client_info.client_subscription_style,
            mca_notifier_ftb_component.subscription_style,
            strlen(mca_notifier_ftb_component.subscription_style));

    /* We try to connect to the FTB backplane now, and we abort
       if we cannot connect for some reason. */
    if (FTB_SUCCESS != (ret = FTB_Connect(&ftb_client_info, &ftb_client_handle))) {
        switch (ret) {
        case FTB_ERR_SUBSCRIPTION_STYLE:
            orte_show_help("help-orcm-notifier-ftb.txt",
                           "invalid subscription style",
                           true, ftb_client_info.client_subscription_style);
            break;

        case FTB_ERR_INVALID_VALUE:
            orte_show_help("help-orcm-notifier-ftb.txt",
                           "invalid value",
                           true);
            break;

        default:
            orte_show_help("help-orcm-notifier-ftb.txt",
                           "ftb connect failed",
                           true);
        }

        return ORCM_ERR_NOT_FOUND;
    }

    *priority = 10;
    *module = (mca_base_module_t *)&orcm_notifier_ftb_module;

    return ORCM_SUCCESS;
}

static int orcm_notifier_ftb_register(void)
{

    /* FTB client subscription style */
    mca_notifier_ftb_component.subscription_style = strdup("FTB_SUBSCRIPTION_NONE");
    (void) mca_base_component_var_register(&mca_notifier_ftb_component.super.base_version, "subscription_style",
                                           "FTB client subscription style. "
                                           "Possible values are none, polling, notify and both (polling and notify).",
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_ftb_component.subscription_style);

    /* Priority */
    mca_notifier_ftb_component.priority = 10;
    (void) mca_base_component_var_register(&mca_notifier_ftb_component.super.base_version, "priority",
                                           "Priority of this component",
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &mca_notifier_ftb_component.priority);

    return ORCM_SUCCESS;
}

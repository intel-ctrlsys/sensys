/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015 - 2016      Intel, Inc. All rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal_config.h"
#include "opal/constants.h"
#include "opal/mca/base/base.h"
#include "opal/mca/sec/sec.h"
#include "sec_munge.h"

static int sec_munge_component_open(void);
static int sec_munge_component_query(mca_base_module_t **module, int *priority);
static int sec_munge_component_close(void);
static int sec_munge_component_register(void);
char* authorize_grp = NULL;
/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
opal_sec_base_component_t mca_sec_munge_component = {
    .base_version = {
        OPAL_SEC_BASE_VERSION_1_0_0,

        /* Component name and version */
        .mca_component_name = "munge",
        MCA_BASE_MAKE_VERSION(component, OPAL_MAJOR_VERSION, OPAL_MINOR_VERSION, OPAL_RELEASE_VERSION),

        /* Component open and close functions */
        .mca_open_component = sec_munge_component_open,
        .mca_close_component = sec_munge_component_close,
        .mca_query_component = sec_munge_component_query,
        .mca_register_component_params = sec_munge_component_register,
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};

static int sec_munge_component_register(void)
{
    char *help_msg = NULL;
    int ret = 0;
    ret = asprintf( &help_msg,
        "Name of the user group defined by system admin who are authorized to "
        "connect to this running daemon");
    if (0 > ret) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }
    ret = mca_base_component_var_register (&mca_sec_munge_component.base_version,
                                           "authorize_group","authorize_group",
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0,0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &authorize_grp);
    free(help_msg);  /* release the help message */
    if (0 > ret) {
        return ret;
    }
    ret = mca_base_var_register_synonym (ret, "opal", "sec", "munge", "authorize_group", 0);
    if (0 > ret) {
        return ret;
    }
    return OPAL_SUCCESS;
}

static int sec_munge_component_open(void)
{
    return OPAL_SUCCESS;
}

static int sec_munge_component_query(mca_base_module_t **module, int *priority)
{
    *priority = 10;
    *module = (mca_base_module_t*)&opal_sec_munge_module;
    return OPAL_SUCCESS;
}


static int sec_munge_component_close(void)
{
    return OPAL_SUCCESS;
}

/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orcm/mca/sst/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/sst/base/static-components.h"

/*
 * Global variables
 */
orcm_sst_base_module_t orcm_sst = {0};
char *orcm_sst_base_node_regex = NULL;

static int orcm_sst_base_register(mca_base_register_flag_t flags)
{
    char *node_regex = NULL;
    int ret = 0;

    (void) mca_base_var_register ("orcm", "orcm", NULL, "node_regex",
                               "node_regex of allocated nodes.",
                               MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                               MCA_BASE_VAR_FLAG_INTERNAL,
                               OPAL_INFO_LVL_9,
                               MCA_BASE_VAR_SCOPE_CONSTANT,
                               &orcm_sst_base_node_regex);
    mca_base_var_register_synonym(ret, "orcm", "orcm", NULL,"node_regx", 0);

    return ORCM_SUCCESS;
}

static int orcm_sst_base_close(void)
{
    /* Close the selected component */
    if( NULL != orcm_sst.finalize ) {
        orcm_sst.finalize();
    }

    return mca_base_framework_components_close(&orcm_sst_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_sst_base_open(mca_base_open_flag_t flags)
{
    return mca_base_framework_components_open(&orcm_sst_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, sst, "ORCM Startup/Shutdown Framework",
                           orcm_sst_base_register,
                           orcm_sst_base_open, orcm_sst_base_close,
                           mca_sst_base_static_components, 0);


/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/util/output.h"
#include "opal/mca/base/mca_base_var.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/sst/sst.h"
#include "orcm/mca/sst/orcmsd/sst_orcmsd.h"

int orcm_stepd_base_num_procs = -1;
char *orcm_stepd_base_jobid = NULL;
char *orcm_stepd_base_vpid = NULL;

static int component_register(void);
static int component_open(void);
static int component_close(void);
static int component_query(mca_base_module_t **module, int *priority);

orcm_sst_base_component_t mca_sst_orcmsd_component = {
    {
        ORCM_SST_BASE_VERSION_1_0_0,

        "orcmsd",
        ORCM_MAJOR_VERSION,
        ORCM_MINOR_VERSION,
        ORCM_RELEASE_VERSION,
        component_open,
        component_close,
        component_query,
        component_register,
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};

static int component_register(void)
{
    mca_base_var_enum_t *new_enum;
    int ret;

    orcm_stepd_base_jobid = NULL;
    ret = mca_base_var_register("orcm", "stepd", "base", "jobid", "Procstepd jobid",
                                MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                MCA_BASE_VAR_FLAG_INTERNAL,
                                OPAL_INFO_LVL_9,
                                MCA_BASE_VAR_SCOPE_READONLY, &orcm_stepd_base_jobid);
    mca_base_var_register_synonym(ret, "orcm", "orcm", "stepd", "jobid", 0);

    orcm_stepd_base_vpid = NULL;
    ret = mca_base_var_register("orcm", "stepd", "base", "vpid", "Procstepd vpid",
                                MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                MCA_BASE_VAR_FLAG_INTERNAL,
                                OPAL_INFO_LVL_9,
                                MCA_BASE_VAR_SCOPE_READONLY, &orcm_stepd_base_vpid);
    mca_base_var_register_synonym(ret, "orcm", "orcm", "stepd", "vpid", 0);

    orcm_stepd_base_num_procs = -1;
    ret = mca_base_var_register("orcm", "stepd", "base", "num_procs",
                                "Used to discover the number of procs in the job",
                                MCA_BASE_VAR_TYPE_INT, NULL, 0,
                                MCA_BASE_VAR_FLAG_INTERNAL,
                                OPAL_INFO_LVL_9,
                                MCA_BASE_VAR_SCOPE_READONLY, &orcm_stepd_base_num_procs);
    mca_base_var_register_synonym(ret, "orcm", "orcm", "stepd", "num_procs", 0);

    return ORTE_SUCCESS;

}
static int component_open(void)
{
     return ORCM_SUCCESS;
}

static int component_close(void)
{
    return ORCM_SUCCESS;
}

static int component_query(mca_base_module_t **module, int *priority)
{
    if (ORCM_PROC_IS_HNP || ORCM_PROC_IS_STEPD) {
        *module = (mca_base_module_t*)&orcm_sst_orcmsd_module;
        *priority = 100;
        return ORCM_SUCCESS;
    }

    *module = NULL;
    *priority = 0;
    return ORCM_ERROR;
}

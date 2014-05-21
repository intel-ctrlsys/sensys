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

#include <perl.h>

#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/pvsn/base/base.h"
#include "pvsn_ww.h"

static int init(void);
static void finalize(void);
static void provision(void);

orcm_pvsn_base_module_t orcm_pvsn_wwulf_module = {
    init,
    finalize,
    provision
};

static PerlInterpreter *my_perl;

static int init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    PERL_SYS_INIT3(&argc,&argv,&env);
    my_perl = perl_alloc();
    perl_construct(my_perl);

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (NULL != my_perl) {
        perl_destruct(my_perl);
        perl_free(my_perl);
        PERL_SYS_TERM();
    }
}

static void provision(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:provision",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
}

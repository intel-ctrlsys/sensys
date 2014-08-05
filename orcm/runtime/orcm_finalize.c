/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/runtime/opal.h"
#include "opal/util/output.h"

#include "orte/mca/ess/base/base.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/sst/base/base.h"

#include "orte/runtime/runtime.h"
#include "runtime/runtime.h"

int orcm_finalize(void)
{
    --orcm_initialized;
    if (0 != orcm_initialized) {
        /* check for mismatched calls */
        if (0 > orcm_initialized) {
            opal_output(0, "%s MISMATCHED CALLS TO ORCM FINALIZE",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        }
        return ORCM_ERROR;
    }
    /* mark that orte is finalizing so that system will work correctly */
    orte_finalizing = true;

    /* everyone must finalize and close the cfgi framework */
    (void) mca_base_framework_close(&orcm_cfgi_base_framework);

    /* cleanup any globals */
    if (NULL != orcm_clusters) {
        OPAL_LIST_RELEASE(orcm_clusters);
    }
    if (NULL != orcm_schedulers) {
        OPAL_LIST_RELEASE(orcm_schedulers);
    }

    (void)orte_ess.finalize();

    /* close the ess itself */
    (void) mca_base_framework_close(&orte_ess_base_framework);

    /* close the sst itself */
    (void) mca_base_framework_close(&orcm_sst_base_framework);

    /* cleanup the process info */
    orte_proc_info_finalize();

    orte_initialized = false;

    /* Close the general debug stream */
    opal_output_close(orte_debug_output);

    /* finalize the opal utilities */
    opal_finalize();

    return ORCM_SUCCESS;
}

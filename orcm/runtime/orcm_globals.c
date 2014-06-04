/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/scd/base/base.h"

int orcm_dt_init(void)
{
    int rc;
    opal_data_type_t tmp;

    /* register the scheduler types for packing/unpacking services */
    tmp = ORCM_ALLOC;
    if (OPAL_SUCCESS !=
        (rc = opal_dss.register_type(orcm_pack_alloc,
                                     orcm_unpack_alloc,
                                     (opal_dss_copy_fn_t)orcm_copy_alloc,
                                     (opal_dss_compare_fn_t)orcm_compare_alloc,
                                     (opal_dss_print_fn_t)orcm_print_alloc,
                                     OPAL_DSS_STRUCTURED,
                                     "ORCM_ALLOC", &tmp))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    return ORCM_SUCCESS;
}

/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_progress.h"
#include "orcm/mca/diag/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/diag/base/static-components.h"

/*
 * Global variables
 */
orcm_diag_API_module_t orcm_diag = {
    orcm_diag_base_calibrate,
    orcm_diag_base_diag_read,
    orcm_diag_base_diag_check
};
orcm_diag_base_t orcm_diag_base;

/* local vars */
static void* progress_thread_engine(opal_object_t *obj);

static int orcm_diag_base_close(void)
{
    if (orcm_diag_base.ev_active) {
        orcm_diag_base.ev_active = false;
        /* stop the thread */
        orcm_stop_progress_thread("diag", true);
    }

    /* deconstruct the base objects */
    OPAL_LIST_DESTRUCT(&orcm_diag_base.modules);
    OBJ_DESTRUCT(&orcm_diag_base.bucket);

    return mca_base_framework_components_close(&orcm_diag_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_diag_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* setup the base objects */
    orcm_diag_base.ev_active = false;
    OBJ_CONSTRUCT(&orcm_diag_base.modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_diag_base.bucket, opal_buffer_t);

    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_diag_base_framework, flags))) {
        return rc;
    }

    /* create the event base and start the progress thread */
    orcm_diag_base.ev_active = true;
    if (NULL == (orcm_diag_base.ev_base = orcm_start_progress_thread("diag", progress_thread_engine))) {
        orcm_diag_base.ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, diag, NULL, NULL,
                           orcm_diag_base_open, orcm_diag_base_close,
                           mca_diag_base_static_components, 0);

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orcm_diag_base.ev_active) {
        opal_event_loop(orcm_diag_base.ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

OBJ_CLASS_INSTANCE(orcm_diag_active_module_t,
                   opal_list_item_t,
                   NULL, NULL);

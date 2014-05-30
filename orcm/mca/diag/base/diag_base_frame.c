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
#include "opal/threads/threads.h"
#include "opal/util/output.h"
#include "opal/util/fd.h"

#include "orte/mca/errmgr/errmgr.h"

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
    orcm_diag_base_calibrate
};
orcm_diag_base_t orcm_diag_base;

/* local vars */
static opal_thread_t progress_thread;
static void* progress_thread_engine(opal_object_t *obj);
static bool progress_thread_running = false;
static opal_event_t blocking_ev;
static int block_pipe[2];

static int orcm_diag_base_close(void)
{
    if (progress_thread_running) {
        orcm_diag_base.ev_active = false;
        /* break the event loop */
        opal_event_base_loopbreak(orcm_diag_base.ev_base);
        /* wait for thread to exit */
        opal_thread_join(&progress_thread, NULL);
        OBJ_DESTRUCT(&progress_thread);
        progress_thread_running = false;
        opal_event_base_free(orcm_diag_base.ev_base);
        close(block_pipe[1]);
    }

    /* deconstruct the base objects */
    OPAL_LIST_DESTRUCT(&orcm_diag_base.modules);
    OBJ_DESTRUCT(&orcm_diag_base.bucket);

    return mca_base_framework_components_close(&orcm_diag_base_framework, NULL);
}

static void wakeup(int fd, short args, void *cbdata)
{
    opal_output_verbose(10, orcm_diag_base_framework.framework_output,
                        "%s diag:wakeup invoked",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
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

    /* create the event base */
    orcm_diag_base.ev_base = opal_event_base_create();
    orcm_diag_base.ev_active = true;
    /* add an event it can block on */
    if (0 > pipe(block_pipe)) {
        ORTE_ERROR_LOG(ORTE_ERR_IN_ERRNO);
        return ORTE_ERR_IN_ERRNO;
    }
    /* Make sure the pipe FDs are set to close-on-exec so that
       they don't leak into children */
    if (opal_fd_set_cloexec(block_pipe[0]) != OPAL_SUCCESS ||
        opal_fd_set_cloexec(block_pipe[1]) != OPAL_SUCCESS) {
        close(block_pipe[0]);
        close(block_pipe[1]);
        ORTE_ERROR_LOG(ORTE_ERR_IN_ERRNO);
        return ORTE_ERR_IN_ERRNO;
    }
    opal_event_set(orcm_diag_base.ev_base, &blocking_ev, block_pipe[0], OPAL_EV_READ, wakeup, NULL);
    opal_event_add(&blocking_ev, 0);

    /* construct the thread object */
    OBJ_CONSTRUCT(&progress_thread, opal_thread_t);
    /* fork off a thread to progress it */
    progress_thread.t_run = progress_thread_engine;
    progress_thread_running = true;
    if (OPAL_SUCCESS != (rc = opal_thread_start(&progress_thread))) {
        ORTE_ERROR_LOG(rc);
        progress_thread_running = false;
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

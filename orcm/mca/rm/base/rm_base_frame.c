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

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/rm/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/rm/base/static-components.h"

/*
 * Global variables
 */
orcm_rm_base_t orcm_rm_base;

/* local vars */
static opal_thread_t progress_thread;
static void* progress_thread_engine(opal_object_t *obj);
static bool progress_thread_running = false;

static int orcm_rm_base_register(mca_base_register_flag_t flags)
{
    /* do we want to just test the resource manager? */
    orcm_rm_base.test_mode = false;
    (void) mca_base_var_register("orcm", "rm", "base", "test_mode",
                                 "Test the ORCM resource manager",
                                 MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_rm_base.test_mode);
    return OPAL_SUCCESS;
}

static int orcm_rm_base_close(void)
{
    if (progress_thread_running) {
        orcm_rm_base.ev_active = false;
        /* break the event loop */
        opal_event_base_loopbreak(orcm_rm_base.ev_base);
        /* wait for thread to exit */
        opal_thread_join(&progress_thread, NULL);
        OBJ_DESTRUCT(&progress_thread);
        progress_thread_running = false;
        opal_event_base_free(orcm_rm_base.ev_base);
    }

    /* deconstruct the base objects */
    OPAL_LIST_DESTRUCT(&orcm_rm_base.active_modules);
    OPAL_LIST_DESTRUCT(&orcm_rm_base.states);

    return mca_base_framework_components_close(&orcm_rm_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_rm_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* setup the base objects */
    orcm_rm_base.ev_active = false;
    OBJ_CONSTRUCT(&orcm_rm_base.active_modules, opal_list_t);
    OBJ_CONSTRUCT(&orcm_rm_base.nodes, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_rm_base.nodes, 8, INT_MAX, 8);

    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_rm_base_framework, flags))) {
        return rc;
    }

    /* create the event base */
    orcm_rm_base.ev_base = opal_event_base_create();
    orcm_rm_base.ev_active = true;
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

MCA_BASE_FRAMEWORK_DECLARE(orcm, rm, NULL, orcm_rm_base_register,
                           orcm_rm_base_open, orcm_rm_base_close,
                           mca_rm_base_static_components, 0);

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orcm_rm_base.ev_active) {
        opal_event_loop(orcm_rm_base.ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

/****    CLASS INSTANTIATIONS    ****/
static void rm_des(orcm_rm_base_active_module_t *s)
{
    if (NULL != s->module->finalize) {
        s->module->finalize();
    }
}
OBJ_CLASS_INSTANCE(orcm_rm_base_active_module_t,
                   opal_list_item_t,
                   NULL, rm_des);

OBJ_CLASS_INSTANCE(orcm_resource_caddy_t,
                   opal_object_t,
                   NULL, NULL);

static void res_con(orcm_resource_t *p)
{
    p->constraint = NULL;
}
static void res_des(orcm_resource_t *p)
{
    if (NULL != p->constraint) {
        free(p->constraint);
    }
}
OBJ_CLASS_INSTANCE(orcm_resource_t,
                   opal_list_item_t,
                   res_con, res_des);

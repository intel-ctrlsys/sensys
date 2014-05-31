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
#include "opal/util/fd.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/runtime/orcm_progress.h"
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
orcm_rm_API_module_t orcm_rm = {
    orcm_rm_base_activate_session_state
};
orcm_rm_base_t orcm_rm_base;

/* local vars */
static void* progress_thread_engine(opal_object_t *obj);

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
    if (orcm_rm_base.ev_active) {
        orcm_rm_base.ev_active = false;
        /* stop the thread */
        orcm_stop_progress_thread("rm", true);
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
    OBJ_CONSTRUCT(&orcm_rm_base.states, opal_list_t);
    OBJ_CONSTRUCT(&orcm_rm_base.nodes, opal_pointer_array_t);
    opal_pointer_array_init(&orcm_rm_base.nodes, 8, INT_MAX, 8);

    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_rm_base_framework, flags))) {
        return rc;
    }

    /* create the event base */
    orcm_rm_base.ev_active = true;
    if (NULL == (orcm_rm_base.ev_base = orcm_start_progress_thread("rm", progress_thread_engine))) {
        orcm_rm_base.ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
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

const char *orcm_rm_session_state_to_str(orcm_rm_session_state_t state)
{
    char *s;

    switch (state) {
    case ORCM_SESSION_STATE_UNDEF:
        s = "UNDEF";
        break;
    case ORCM_SESSION_STATE_REQ:
        s = "REQUESTING RESOURCES";
        break;
    case ORCM_SESSION_STATE_ACTIVE:
        s = "LAUNCHING SESSION";
        break;
    case ORCM_SESSION_STATE_KILL:
        s = "KILLING SESSION";
        break;
    default:
        s = "UNKNOWN";
    }
    return s;
}

const char *orcm_rm_node_state_to_str(orcm_node_state_t state)
{
    char *s;

    switch (state) {
    case ORCM_NODE_STATE_UNDEF:
        s = "UNDEF";
        break;
    case ORCM_NODE_STATE_UNKNOWN:
        s = "UNKNOWN";
        break;
    case ORCM_NODE_STATE_UP:
        s = "UP";
        break;
    case ORCM_NODE_STATE_DOWN:
        s = "DOWN";
        break;
    case ORCM_NODE_STATE_SESTERM:
        s = "SESSION TERMINATING";
        break;
    default:
        s = "STATEUNDEF";
    }
    return s;
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

OBJ_CLASS_INSTANCE(orcm_rm_state_t,
                   opal_list_item_t,
                   NULL, NULL);

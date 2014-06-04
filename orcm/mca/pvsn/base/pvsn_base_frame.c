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

#include "orcm/runtime/orcm_progress.h"
#include "orcm/mca/pvsn/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/pvsn/base/static-components.h"

/*
 * Global variables
 */
orcm_pvsn_API_module_t orcm_pvsn = {
    orcm_pvsn_base_query_avail,
    orcm_pvsn_base_query_status,
    orcm_pvsn_base_provision
};
orcm_pvsn_base_t orcm_pvsn_base;

/* local vars */
static void* progress_thread_engine(opal_object_t *obj);

static int orcm_pvsn_base_close(void)
{
    if (orcm_pvsn_base.ev_active) {
        orcm_pvsn_base.ev_active = false;
        /* stop the thread */
        orcm_stop_progress_thread("pvsn", true);
    }

    return mca_base_framework_components_close(&orcm_pvsn_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_pvsn_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* setup the base objects */
    orcm_pvsn_base.ev_active = false;

    if (OPAL_SUCCESS != (rc = mca_base_framework_components_open(&orcm_pvsn_base_framework, flags))) {
        return rc;
    }

    /* create the event base */
    orcm_pvsn_base.ev_active = true;
    if (NULL == (orcm_pvsn_base.ev_base = orcm_start_progress_thread("pvsn", progress_thread_engine))) {
        orcm_pvsn_base.ev_active = false;
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, pvsn, NULL, NULL,
                           orcm_pvsn_base_open, orcm_pvsn_base_close,
                           mca_pvsn_base_static_components, 0);

static void* progress_thread_engine(opal_object_t *obj)
{
    while (orcm_pvsn_base.ev_active) {
        opal_event_loop(orcm_pvsn_base.ev_base, OPAL_EVLOOP_ONCE);
    }
    return OPAL_THREAD_CANCELLED;
}

/***   CLASS INSTANTIATIONS   ***/
static void imcon(orcm_pvsn_image_t *p)
{
    p->name = NULL;
    OBJ_CONSTRUCT(&p->attributes, opal_list_t);
}
static void imdes(orcm_pvsn_image_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    OPAL_LIST_DESTRUCT(&p->attributes);
}
OBJ_CLASS_INSTANCE(orcm_pvsn_image_t,
                   opal_list_item_t,
                   imcon, imdes);

static void pvcon(orcm_pvsn_provision_t *p)
{
    p->nodes = NULL;
    OBJ_CONSTRUCT(&p->image, orcm_pvsn_image_t);
}
static void pvdes(orcm_pvsn_provision_t *p)
{
    if (NULL != p->nodes) {
        free(p->nodes);
    }
    OBJ_DESTRUCT(&p->image);
}
OBJ_CLASS_INSTANCE(orcm_pvsn_provision_t,
                   opal_list_item_t,
                   pvcon, pvdes);

static void cdcon(orcm_pvsn_caddy_t *p)
{
    p->nodes = NULL;
    p->image = NULL;
    p->attributes = NULL;
    p->pvsn_cbfunc = NULL;
    p->query_cbfunc = NULL;
    p->cbdata = NULL;
    p->next_stage = NULL;
}
OBJ_CLASS_INSTANCE(orcm_pvsn_caddy_t,
                   opal_object_t,
                   cdcon, NULL);

static void rscon(orcm_pvsn_resource_t *p)
{
    OBJ_CONSTRUCT(&p->kv, opal_value_t);
}
static void rsdes(orcm_pvsn_resource_t *p)
{
    OBJ_DESTRUCT(&p->kv);
}
OBJ_CLASS_INSTANCE(orcm_pvsn_resource_t,
                   opal_list_item_t,
                   rscon, rsdes);

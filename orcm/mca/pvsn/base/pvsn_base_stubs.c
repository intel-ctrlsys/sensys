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
#include "orcm/types.h"

#include "opal/mca/mca.h"
#include "opal/util/output.h"

#include "orte/types.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/pvsn/base/base.h"

static void query_avail(int fd, short args, void *cbdata)
{
    orcm_pvsn_caddy_t *caddy = (orcm_pvsn_caddy_t*)cbdata;
    int rc=ORCM_ERR_NOT_SUPPORTED;
    opal_list_t images;

    OBJ_CONSTRUCT(&images, opal_list_t);
    if (NULL != orcm_pvsn_base.active.get_available) {
        rc = orcm_pvsn_base.active.get_available(caddy->nodes, &images);
    }

    if (NULL != caddy->query_cbfunc) {
        caddy->query_cbfunc(rc, &images, caddy->cbdata);
    }
    OPAL_LIST_DESTRUCT(&images);
    OBJ_RELEASE(caddy);
}

void orcm_pvsn_base_query_avail(char *resources,
                                orcm_pvsn_query_cbfunc_t cbfunc,
                                void *cbdata)
{
    orcm_pvsn_caddy_t *caddy;

    /* push the request into the event thread */
    caddy = OBJ_NEW(orcm_pvsn_caddy_t);
    caddy->nodes = resources;  // reuse this field
    caddy->query_cbfunc = cbfunc;
    caddy->cbdata = cbdata;
    opal_event_set(orcm_pvsn_base.ev_base,
                   &caddy->ev, -1, OPAL_EV_WRITE,
                   query_avail, caddy);
    opal_event_set_priority(&caddy->ev, ORTE_MSG_PRI);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}

static void query_status(int fd, short args, void *cbdata)
{
    orcm_pvsn_caddy_t *caddy = (orcm_pvsn_caddy_t*)cbdata;
    int rc=ORCM_ERR_NOT_SUPPORTED;
    opal_list_t images;

    OBJ_CONSTRUCT(&images, opal_list_t);
    if (NULL != orcm_pvsn_base.active.get_status) {
        rc = orcm_pvsn_base.active.get_status(caddy->nodes, &images);
    }

    if (NULL != caddy->query_cbfunc) {
        caddy->query_cbfunc(rc, &images, caddy->cbdata);
    }
    OPAL_LIST_DESTRUCT(&images);
    OBJ_RELEASE(caddy);
}

void orcm_pvsn_base_query_status(char *nodes,
                                 orcm_pvsn_query_cbfunc_t cbfunc,
                                 void *cbdata)
{
    orcm_pvsn_caddy_t *caddy;

    /* push the request into the event thread */
    caddy = OBJ_NEW(orcm_pvsn_caddy_t);
    caddy->nodes = nodes;
    caddy->query_cbfunc = cbfunc;
    caddy->cbdata = cbdata;
    opal_event_set(orcm_pvsn_base.ev_base,
                   &caddy->ev, -1, OPAL_EV_WRITE,
                   query_status, caddy);
    opal_event_set_priority(&caddy->ev, ORTE_MSG_PRI);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}

static void provision(int fd, short args, void *cbdata)
{
    orcm_pvsn_caddy_t *caddy = (orcm_pvsn_caddy_t*)cbdata;
    int rc=ORCM_ERR_NOT_SUPPORTED;

    if (NULL != orcm_pvsn_base.active.provision) {
        rc = orcm_pvsn_base.active.provision(caddy->nodes,
                                             caddy->image,
                                             caddy->attributes);
    }

    if (NULL != caddy->pvsn_cbfunc) {
        caddy->pvsn_cbfunc(rc, caddy->cbdata);
    }
    OBJ_RELEASE(caddy);
}

void orcm_pvsn_base_provision(char *nodes,
                              char *image,
                              opal_list_t *attributes,
                              orcm_pvsn_provision_cbfunc_t cbfunc,
                              void *cbdata)
{
    orcm_pvsn_caddy_t *caddy;

    /* push the request into the event thread */
    caddy = OBJ_NEW(orcm_pvsn_caddy_t);
    caddy->nodes = nodes;
    caddy->image = image;
    caddy->attributes = attributes;
    caddy->pvsn_cbfunc = cbfunc;
    caddy->cbdata = cbdata;
    opal_event_set(orcm_pvsn_base.ev_base,
                   &caddy->ev, -1, OPAL_EV_WRITE,
                   provision, caddy);
    opal_event_set_priority(&caddy->ev, ORTE_MSG_PRI);
    opal_event_active(&caddy->ev, OPAL_EV_WRITE, 1);
}

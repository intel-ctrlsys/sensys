/*
 * Copyright (c) 2013      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <stddef.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "opal/dss/dss.h"
#include "opal/class/opal_hash_table.h"
#include "opal/class/opal_bitmap.h"
#include "opal/runtime/opal_progress.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/ess/ess.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/state/state.h"
#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/runtime.h"
#include "orte/runtime/data_type_support/orte_dt_support.h"

#include "orte/mca/rml/base/rml_contact.h"

#include "orte/mca/routed/base/base.h"
#include "routed_orcm.h"

/* the orcm module supports a network of daemons that host
 * no local apps - they are simply monitoring the system
 * resource usage. Thus, they use static ports to wire
 * themselves up to a set of identified masters via a
 * radix topology. If a peer dies, the daemon will rewire
 * the network by reconnecting to the downstream peer(s).
 */

static int init(void);
static int finalize(void);
static int delete_route(orte_process_name_t *proc);
static int update_route(orte_process_name_t *target,
                        orte_process_name_t *route);
static orte_process_name_t get_route(orte_process_name_t *target);
static int init_routes(orte_jobid_t job, opal_buffer_t *ndat);
static int route_lost(const orte_process_name_t *route);
static bool route_is_defined(const orte_process_name_t *target);
static void update_routing_plan(void);
static void get_routing_list(orte_grpcomm_coll_t type,
                             orte_grpcomm_collective_t *coll);
static int get_wireup_info(opal_buffer_t *buf);
static int set_lifeline(orte_process_name_t *proc);
static size_t num_routes(void);

#if OPAL_ENABLE_FT_CR == 1
static int orcm_ft_event(int state);
#endif

orte_routed_module_t orte_routed_orcm_module = {
    init,
    finalize,
    delete_route,
    update_route,
    get_route,
    init_routes,
    route_lost,
    route_is_defined,
    set_lifeline,
    update_routing_plan,
    get_routing_list,
    get_wireup_info,
    num_routes,
#if OPAL_ENABLE_FT_CR == 1
    orcm_ft_event
#else
    NULL
#endif
};

/* local tracker */
typedef struct {
    opal_list_item_t super;
    orte_process_name_t cntlr;
    bool alive;
    opal_list_t nodes;  // orte_namelist_t
} rack_t;
static void rk_con(rack_t *p)
{
    OBJ_CONSTRUCT(&p->nodes, opal_list_t);
    p->alive = true;
}
static void rk_des(rack_t *p)
{
    OPAL_LIST_DESTRUCT(&p->nodes);
}
OBJ_CLASS_INSTANCE(rack_t,
                   opal_list_item_t,
                   rk_con, rk_des);
typedef struct {
    opal_list_item_t super;
    orte_process_name_t cntlr;
    bool alive;
    opal_list_t racks;  // rack_t
} row_t;
static void rw_con(row_t *p)
{
    OBJ_CONSTRUCT(&p->racks, opal_list_t);
    p->alive = true;
}
static void rw_des(row_t *p)
{
    OPAL_LIST_DESTRUCT(&p->racks);
}
OBJ_CLASS_INSTANCE(row_t,
                   opal_list_item_t,
                   rw_con, rw_des);

/* local globals */
static orte_namelist_t *lifeline=NULL;
static opal_list_t cluster;


static int init(void)
{
    lifeline = NULL;
    
    if (ORTE_PROC_IS_TOOL) {
        return ORTE_SUCCESS;
    }

    /* setup the tracking list of who we've been
     * told about
     */
    OBJ_CONSTRUCT(&cluster, opal_list_t);

    return ORTE_SUCCESS;
}

static int finalize(void)
{
    if (!ORTE_PROC_IS_TOOL) {
        OPAL_LIST_DESTRUCT(&cluster);
    }

    return ORTE_SUCCESS;
}

static int delete_route(orte_process_name_t *proc)
{
    /* There is nothing to do here. The routes will be
     * redefined when we update the routing tree
     */
    
    return ORTE_SUCCESS;
}

static int update_route(orte_process_name_t *target,
                        orte_process_name_t *route)
{
    /* nothing to do here */
    return ORTE_SUCCESS;
}


static orte_process_name_t get_route(orte_process_name_t *target)
{
    orte_process_name_t *ret;

    if (!orte_routing_is_enabled || ORTE_PROC_IS_TOOL) {
        ret = target;
        goto found;
    }

    /* if the target is me, then send direct to the target! */
    if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                    target, ORTE_PROC_MY_NAME)) {
        ret = target;
        goto found;
    }

    /* if the target is from outside my jobid, send direct */
    if (target->jobid != ORTE_PROC_MY_NAME->jobid) {
        ret = target;
        goto found;
    }

    /* if I am a compute node daemon, then my route is
     * always thru my lifeline
     */
    if (ORTE_PROC_IS_DAEMON) {
        /* our route is our lifeline */
        if (NULL != lifeline) {
            ret = &lifeline->target;
        } else {
            /* if we get here, then we didn't find the route */
            ret = ORTE_NAME_INVALID;
        }
    } else if (ORTE_PROC_IS_AGGREGATOR) {
    } else if (ORTE_PROC_IS_SCHEDULER) {
        /* my route is always to the first available
         * row controller - if no rows are defined,
         * then to the first available rack controller.
         * If we don't have any of those, then direct
         */
    }

found:
    OPAL_OUTPUT_VERBOSE((1, orte_routed_base_framework.framework_output,
                         "%s routed_orcm_get(%s) --> %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(target), 
                         ORTE_NAME_PRINT(ret)));
    
    return *ret;
}

static int init_routes(orte_jobid_t job, opal_buffer_t *ndat)
{
    /* the cluster definition comes to us in a set of packed
     * opal_buffer_t objects
     */

    return ORTE_SUCCESS;
}

static int route_lost(const orte_process_name_t *route)
{
    char *ctmp;
    time_t now;
    local_route_t *rt, *rptr, *r2;

    OPAL_OUTPUT_VERBOSE((2, orte_routed_base_framework.framework_output,
                         "%s route to %s lost",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(route)));

    /* tools don't route, so nothing to do here */
    if (ORTE_PROC_IS_TOOL) {
        return ORTE_SUCCESS;
    }

    /* if we are a master, just ignore this - we let everyone
     * in the orcm groups reconnect as required
     */
    if (ORTE_PROC_IS_SCHEDULER) {
        return ORTE_SUCCESS;
    }

    /* ensure a log message is delivered and printed on the orcm master */
    now = time(NULL);
    ctmp = ctime(&now);
    ctmp[strlen(ctmp)-1] = '\0';
    orte_show_help("help-routed-orcm.txt", "lost-route", true,
                   ctmp, orte_process_info.nodename,
                   ORTE_NAME_PRINT(route));

    if (orte_finalizing) {
        /* make no attempt to recover */
        return ORTE_SUCCESS;
    }

    /* see if this proc is in our routing table */
    r2 = NULL;
    OPAL_LIST_FOREACH(rptr, &routes, local_route_t) {
        if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                        route, &rptr->target)) {
            r2 = rptr;
            goto found;
        }
        OPAL_LIST_FOREACH(rt, &rptr->routes, local_route_t) {
            if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                        route, &rt->target)) {
                r2 = rt;
                goto found;
            }
        }
    }
 found:
    if (NULL == r2) {
        /* don't care */
        return ORTE_SUCCESS;
    }
    /* mark the route as down */
    r2->alive = false;

    /* if we don't have a lifeline, or this isn't
     * the lifeline, then we don't care
     */
    if (NULL == lifeline || r2 != lifeline) {
        return ORTE_SUCCESS;
    }

    /* if we lose the connection to the lifeline and we are NOT already,
     * in finalize, attempt to rewire to a peer of the lifeline 
     */
    OPAL_LIST_FOREACH(rt, &lifeline->routes, local_route_t) {
        if (rt->alive) {
            lifeline = rt;
            return ORTE_SUCCESS;
        }
    }
 
    /* if we can't find a replacement, then error */
    return ORTE_ERR_FATAL;
}

static bool route_is_defined(const orte_process_name_t *target)
{
    /* by definition, we always have a route */
    return true;
}

static int set_lifeline(orte_process_name_t *proc)
{
    local_route_t *rt, *rptr;

    /* find this proc in our routes */
    OPAL_LIST_FOREACH(rt, &routes, local_route_t) {
        if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                        &rt->target, proc)) {
            lifeline = rt;
            return ORTE_SUCCESS;
        }
        OPAL_LIST_FOREACH(rptr, &rt->routes, local_route_t) {
            if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL,
                                                            &rptr->target, proc)) {
            lifeline = rptr;
            return ORTE_SUCCESS;
            }
        }
    }
    
    return ORTE_ERR_NOT_FOUND;
}

static void update_routing_plan(void)
{
    return;
}

static void get_routing_list(orte_grpcomm_coll_t type,
                             orte_grpcomm_collective_t *coll)
{
    /* irrelevant for us */
    return;
}

static int get_wireup_info(opal_buffer_t *buf)
{
    /* we use static ports, so just return */
    return ORTE_SUCCESS;
}

static size_t num_routes(void)
{
    return 0;
}

#if OPAL_ENABLE_FT_CR == 1
static int orcm_ft_event(int state)
{
    int ret, exit_status = ORTE_SUCCESS;

    /******** Checkpoint Prep ********/
    if(OPAL_CRS_CHECKPOINT == state) {
    }
    /******** Continue Recovery ********/
    else if (OPAL_CRS_CONTINUE == state ) {
    }
    /******** Restart Recovery ********/
    else if (OPAL_CRS_RESTART == state ) {
        /*
         * Re-exchange the routes
         */
        if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, NULL))) {
            exit_status = ret;
            goto cleanup;
        }
    }
    else if (OPAL_CRS_TERM == state ) {
        /* Nothing */
    }
    else {
        /* Error state = Nothing */
    }

 cleanup:
    return exit_status;
}
#endif


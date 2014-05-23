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

/* local globals */
static orte_process_name_t *lifeline=NULL;
static opal_list_t my_children;  // orte_routed_tree_t's

static int init(void)
{
    lifeline = NULL;
    
    if (ORTE_PROC_IS_TOOL) {
        return ORTE_SUCCESS;
    }

    /* setup the tracking list of who we've been
     * told about
     */
    OBJ_CONSTRUCT(&my_children, opal_list_t);

    return ORTE_SUCCESS;
}

static int finalize(void)
{
    if (!ORTE_PROC_IS_TOOL) {
        OPAL_LIST_DESTRUCT(&my_children);
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
    orte_process_name_t *ret, daemon;
    orte_routed_tree_t *child;

    /* if I am a tool, or routing isn't enabled, go direct */
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

    /* if the target is from a different jobid, go direct */
    if (target->jobid != ORTE_PROC_MY_NAME->jobid) {
        ret = target;
        goto found;
    }

    daemon.jobid = ORTE_PROC_MY_NAME->jobid;
    /* if I am on a compute node, I can only go upward */
    if (ORTE_PROC_IS_DAEMON) {
        daemon.vpid = ORTE_PROC_MY_PARENT->vpid;
        ret = &daemon;
        goto found;
    }

    /* get here if I am an aggregator, which means I am a
     * row or rack controller. My first step is to see if
     * the target is beneath me - if so, route thru the
     * appropriate child. In the case of a rack controller,
     * this will be a direct route as the compute nodes
     * directly connect to me (for now).
     */
    OPAL_LIST_FOREACH(child, &my_children, orte_routed_tree_t) {
        if (child->vpid == target->vpid) {
            /* the child is the target - send it directly there */
            ret = target;
            goto found;
        }
        /* otherwise, see if the target is below the child */
        if (opal_bitmap_is_set_bit(&child->relatives, target->vpid)) {
            /* yep - we need to step through this child */
            daemon.vpid = child->vpid;
            ret = &daemon;
            goto found;
        }
    }

    /* if we get here, then the target is not beneath
     * any of our children, so we have to step up through our parent
     */
    daemon.vpid = ORTE_PROC_MY_PARENT->vpid;
    ret = &daemon;

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
    opal_buffer_t *row, *rack, *sched;
    orte_process_name_t name, sched_name, cluster_name, row_name, rack_name;
    int32_t i, j, nrows, nracks, cnt;
    int rc;
    bool sched_present = false;
    bool cluster_ctlr_present = false;
    bool row_ctlr_present = false;
    bool rack_ctlr_present = false;
    bool i_am_sched = false;
    bool i_am_cluster_ctlr = false;
    bool i_am_row_ctlr = false;
    bool i_am_rack_ctlr = false;
    orte_routed_tree_t *child = NULL;

    if (NULL == ndat || job != ORTE_PROC_MY_NAME->jobid) {
        /* nothing to do */
        return ORTE_SUCCESS;
    }

    /* the cluster definition comes to us in a set of packed
     * opal_buffer_t objects, so unpack them in order */
    
    /* first unpack the scheduler section */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(ndat, &sched, &cnt, OPAL_BUFFER))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* see if a scheduler is present */
    cnt = 1;
    if (OPAL_SUCCESS == (rc = opal_dss.unpack(sched, &sched_name, &cnt, ORTE_NAME)) &&
        ORTE_JOBID_INVALID != sched_name.jobid &&
        ORTE_VPID_INVALID != sched_name.vpid) {
        /* yep - mark it */
        sched_present = true;
    } else if (OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(sched);
        return rc;
    }
    OBJ_RELEASE(sched);
    if (sched_name.jobid == ORTE_PROC_MY_NAME->jobid &&
        sched_name.vpid == ORTE_PROC_MY_NAME->vpid) {
        i_am_sched = true;
    }

    /* if we are a tool, we just point to the scheduler, if present,
     * so that we will commit suicide if the scheduler dies */
    if (ORTE_PROC_IS_TOOL) {
        if (sched_present) {
            ORTE_PROC_MY_PARENT->jobid = sched_name.jobid;
            ORTE_PROC_MY_PARENT->vpid = sched_name.vpid;
        }
        return ORTE_SUCCESS;
    }

    /* unpack the process name of the cluster controller */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(ndat, &cluster_name, &cnt, ORTE_NAME))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (ORTE_JOBID_INVALID != cluster_name.jobid &&
        ORTE_VPID_INVALID != cluster_name.vpid) {
        /* a cluster controller is present */
        cluster_ctlr_present = true;
        if (i_am_sched) {
            /* I am the scheduler - all cluster controllers are my children */
            opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                "%s init_routes: I am scheduler - adding cluster %s as my direct child",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                ORTE_NAME_PRINT(&cluster_name));
            child = OBJ_NEW(orte_routed_tree_t);
            child->vpid = cluster_name.vpid;
            opal_bitmap_init(&child->relatives, orte_process_info.num_procs);
            opal_list_append(&my_children, &child->super);
        }
    } else if (cluster_name.jobid == ORTE_PROC_MY_NAME->jobid &&
               cluster_name.vpid == ORTE_PROC_MY_NAME->vpid) {
        /* it's me - so the row controllers will be my direct children */
        i_am_cluster_ctlr = true;
    }

    /* and the number of rows in the system */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(ndat, &nrows, &cnt, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    opal_output_verbose(1, orte_routed_base_framework.framework_output,
                        "%s init_routes: cluster %s has %d rows",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&cluster_name), nrows);

    /* for each row, unpack the row buffer */
    for (i=0; i < nrows; i++) {
        i_am_row_ctlr = false;
        i_am_rack_ctlr = false;
        row_ctlr_present = false;
        rack_ctlr_present = false;
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(ndat, &row, &cnt, OPAL_BUFFER))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        /* unpack the number of racks in this row */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(row, &nracks, &cnt, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        /* get the name of the row controller */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(row, &row_name, &cnt, ORTE_NAME))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        if (ORTE_JOBID_INVALID != row_name.jobid &&
            ORTE_VPID_INVALID != row_name.vpid) {
            row_ctlr_present = true;
            if (row_name.jobid == ORTE_PROC_MY_NAME->jobid &&
                row_name.vpid == ORTE_PROC_MY_NAME->vpid) {
                /* it's me - so the rack controllers in this row will be my direct children */
                i_am_row_ctlr = true;
            }
        }
        opal_output_verbose(1, orte_routed_base_framework.framework_output,
                            "%s init_routes: row %d(%s) has %d racks",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            i, ORTE_NAME_PRINT(&row_name), nracks);

        if (row_ctlr_present) {
            if (i_am_sched && cluster_ctlr_present) {
                /* add this to the current child so it points to it
                 * via the cluster controller */
                opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                    "%s init_routes: I am scheduler with cluster_ctlr - adding row %s as my indirect child",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    ORTE_NAME_PRINT(&row_name));
                opal_bitmap_set_bit(&child->relatives, row_name.vpid);
            } else if (i_am_cluster_ctlr || (!cluster_ctlr_present && i_am_sched)) {
                /* this row will be my direct child */
                opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                    "%s init_routes: I am %s - adding row %s as my direct child",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                    i_am_sched ? "scheduler w/o cluster ctlr" : "cluster ctlr",
                                    ORTE_NAME_PRINT(&row_name));
                child = OBJ_NEW(orte_routed_tree_t);
                child->vpid = row_name.vpid;
                opal_bitmap_init(&child->relatives, orte_process_info.num_procs);
                opal_list_append(&my_children, &child->super);
            }
        }
        if (i_am_row_ctlr) {
            /* point my parent at the cluster controller, if it is around */
            if (cluster_ctlr_present) {
                ORTE_PROC_MY_PARENT->jobid = cluster_name.jobid;
                ORTE_PROC_MY_PARENT->vpid = cluster_name.vpid;
            } else if (sched_present) {
                /* otherwise, use the scheduler if it is available */
                ORTE_PROC_MY_PARENT->jobid = sched_name.jobid;
                ORTE_PROC_MY_PARENT->vpid = sched_name.vpid;
            } else {
                /* just use myself */
                ORTE_PROC_MY_PARENT->jobid = ORTE_PROC_MY_NAME->jobid;
                ORTE_PROC_MY_PARENT->vpid = ORTE_PROC_MY_NAME->vpid;
            }
            /* set the lifeline */
            lifeline = ORTE_PROC_MY_PARENT;
        }
        /* for each rack, unpack the rack buffer */
        cnt = 1;
        for (j=0; j < nracks; j++) {
            opal_output_verbose(1, orte_routed_base_framework.framework_output,
                                "%s init_routes: unpacking rack %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j);
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(row, &rack, &cnt, OPAL_BUFFER))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            /* get the name of the rack controller */
            cnt = 1;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(rack, &rack_name, &cnt, ORTE_NAME))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            if (ORTE_JOBID_INVALID != rack_name.jobid &&
                ORTE_VPID_INVALID != rack_name.vpid) {
                rack_ctlr_present = true;
                if (rack_name.jobid == ORTE_PROC_MY_NAME->jobid &&
                    rack_name.vpid == ORTE_PROC_MY_NAME->vpid) {
                    /* it's me - so the daemons in this rack will be my direct children */
                    i_am_rack_ctlr = true;
                }
            }
            opal_output_verbose(1, orte_routed_base_framework.framework_output,
                                "%s init_routes: rack %d controller %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j,
                                ORTE_NAME_PRINT(&rack_name));
            if (rack_ctlr_present) {
                if (i_am_row_ctlr ||
                    (i_am_cluster_ctlr && !row_ctlr_present) ||
                    (i_am_sched && !cluster_ctlr_present && !row_ctlr_present)) {
                    /* this rack is a direct child of mine */
                    opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                        "%s init_routes: I am %s - adding rack %s as my direct child",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                        (i_am_sched ? "scheduler w/o row or cluster ctlrs" :
                                         (i_am_cluster_ctlr ? "cluster ctlr w/o row ctlr" : "row ctlr")),
                                        ORTE_NAME_PRINT(&rack_name));
                    child = OBJ_NEW(orte_routed_tree_t);
                    child->vpid = rack_name.vpid;
                    opal_bitmap_init(&child->relatives, orte_process_info.num_procs);
                    opal_list_append(&my_children, &child->super);
                } else if (i_am_cluster_ctlr || i_am_sched) {
                    opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                        "%s init_routes: I am %s - adding rack %s as my indirect child",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                        i_am_sched ? "scheduler" : "cluster ctlr",
                                        ORTE_NAME_PRINT(&rack_name));
                    opal_bitmap_set_bit(&child->relatives, rack_name.vpid);
                }
            }
            if (rack_name.jobid == ORTE_PROC_MY_NAME->jobid &&
                rack_name.vpid == ORTE_PROC_MY_NAME->vpid) {
                i_am_rack_ctlr = true;
                /* point my parent at the row controller, if present */
                if (row_ctlr_present) {
                    ORTE_PROC_MY_PARENT->jobid = row_name.jobid;
                    ORTE_PROC_MY_PARENT->vpid = row_name.vpid;
                } else if (cluster_ctlr_present) {
                    /* point to the cluster ctlr if it is around and the row ctlr isn't */
                    ORTE_PROC_MY_PARENT->jobid = cluster_name.jobid;
                    ORTE_PROC_MY_PARENT->vpid = cluster_name.vpid;
                } else if (sched_present) {
                    /* flow up to the scheduler */
                    ORTE_PROC_MY_PARENT->jobid = sched_name.jobid;
                    ORTE_PROC_MY_PARENT->vpid = sched_name.vpid;
                } else {
                    /* just use myself */
                    ORTE_PROC_MY_PARENT->jobid = ORTE_PROC_MY_NAME->jobid;
                    ORTE_PROC_MY_PARENT->vpid = ORTE_PROC_MY_NAME->vpid;
                }
                opal_output_verbose(1, orte_routed_base_framework.framework_output,
                                    "%s init_routes: rack %d controller %s parent %s",
                                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j,
                                    ORTE_NAME_PRINT(&rack_name), ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT));
                /* set the lifeline */
                lifeline = ORTE_PROC_MY_PARENT;
            }
            /* we don't need the number of nodes in the rack - the rack buffer
             * was just packed with names, so read them until the end */
            cnt = 1;
            while (OPAL_SUCCESS == (rc = opal_dss.unpack(rack, &name, &cnt, ORTE_NAME))) {
                if (i_am_rack_ctlr ||
                    (i_am_row_ctlr && !rack_ctlr_present) ||
                    (i_am_cluster_ctlr && !row_ctlr_present && !rack_ctlr_present) ||
                    (i_am_sched && !cluster_ctlr_present && !row_ctlr_present && !rack_ctlr_present)) {
                    /* this node is a child of this rack */
                    opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                        "%s init_routes: I am %s - adding node %s as my direct child",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                        (i_am_sched ? "scheduler w/o rack/row/cluster ctlrs" :
                                         (i_am_cluster_ctlr ? "cluster ctlr w/o rack/row ctlrs" :
                                          (i_am_row_ctlr ? "row ctlr w/o rack ctlr" : "rack ctlr"))),
                                        ORTE_NAME_PRINT(&name));
                    child = OBJ_NEW(orte_routed_tree_t);
                    child->vpid = name.vpid;
                    opal_bitmap_init(&child->relatives, orte_process_info.num_procs);
                    opal_list_append(&my_children, &child->super);
                } else if (i_am_row_ctlr || i_am_cluster_ctlr || i_am_sched) {
                   opal_output_verbose(5, orte_routed_base_framework.framework_output,
                                        "%s init_routes: I am %s - adding node %s as my indirect child",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                       (i_am_sched ? "scheduler" :
                                        (i_am_cluster_ctlr ? "cluster ctlr" : "row ctlr")),
                                        ORTE_NAME_PRINT(&name));
                    opal_bitmap_set_bit(&child->relatives, name.vpid);
                }
                if (ORTE_PROC_MY_NAME->jobid == name.jobid &&
                    ORTE_PROC_MY_NAME->vpid == name.vpid) {
                    /* point my parent at the rack controller, if present */
                    if (rack_ctlr_present) {
                        ORTE_PROC_MY_PARENT->jobid = rack_name.jobid;
                        ORTE_PROC_MY_PARENT->vpid = rack_name.vpid;
                    } else if (row_ctlr_present) {
                        /* fall back to the row controller, if present */
                        ORTE_PROC_MY_PARENT->jobid = row_name.jobid;
                        ORTE_PROC_MY_PARENT->vpid = row_name.vpid;
                    } else if (cluster_ctlr_present) {
                        /* fall back to the cluster controller */
                        ORTE_PROC_MY_PARENT->jobid = cluster_name.jobid;
                        ORTE_PROC_MY_PARENT->vpid = cluster_name.vpid;
                    } else if (sched_present) {
                        /* fall back to the scheduler */
                        ORTE_PROC_MY_PARENT->jobid = sched_name.jobid;
                        ORTE_PROC_MY_PARENT->vpid = sched_name.vpid;
                    } else {
                        /* just use myself */
                        ORTE_PROC_MY_PARENT->jobid = ORTE_PROC_MY_NAME->jobid;
                        ORTE_PROC_MY_PARENT->vpid = ORTE_PROC_MY_NAME->vpid;
                    }
                    opal_output_verbose(1, orte_routed_base_framework.framework_output,
                                        "%s init_routes: node %s parent %s",
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                        ORTE_NAME_PRINT(&name),
                                        ORTE_NAME_PRINT(ORTE_PROC_MY_PARENT));
                    /* set the lifeline */
                    lifeline = ORTE_PROC_MY_PARENT;
                }
                cnt = 1;
            }
            if (OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    }

    if (4 < opal_output_get_verbosity(orte_routed_base_framework.framework_output)) {
        opal_output(0, "%s FINAL ROUTING PLAN: Parent %d #children %d",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    ORTE_PROC_MY_PARENT->vpid, (int)opal_list_get_size(&my_children));
        OPAL_LIST_FOREACH(child, &my_children, orte_routed_tree_t) {
            opal_output(0, "%s: \tchild %d", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), child->vpid);
            for (j=0; j < (int)orte_process_info.num_procs; j++) {
                if (opal_bitmap_is_set_bit(&child->relatives, j)) {
                    opal_output(0, "%s: \t\trelation %d", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j);
                }
            }
        }
    }

    return ORTE_SUCCESS;
}

static int route_lost(const orte_process_name_t *route)
{
    char *ctmp;
    time_t now;

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

    /* if from another jobid, ignore it */
    if (route->jobid != ORTE_PROC_MY_NAME->jobid) {
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

    /* if this is my parent, then reconnect to someone above it */

    /* if this is someone under me, then just record it */

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


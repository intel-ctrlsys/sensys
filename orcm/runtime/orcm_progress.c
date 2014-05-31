/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"
#include "opal/threads/threads.h"
#include "opal/util/fd.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"

#include "orcm/runtime/orcm_progress.h"

/* create a tracking object for progress threads */
typedef struct {
    opal_list_item_t super;
    char *name;
    opal_event_base_t *ev_base;
    bool block_active;
    opal_event_t block;
    bool engine_defined;
    opal_thread_t engine;
    int pipe[2];
} orcm_progress_tracker_t;
static void trkcon(orcm_progress_tracker_t *p)
{
    p->name = NULL;
    p->ev_base = NULL;
    p->block_active = false;
    p->engine_defined = false;
    p->pipe[0] = -1;
    p->pipe[1] = -1;
}
static void trkdes(orcm_progress_tracker_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    if (p->block_active) {
        opal_event_del(&p->block);
    }
    if (NULL != p->ev_base) {
        opal_event_base_free(p->ev_base);
    }
    if (0 <= p->pipe[0]) {
        close(p->pipe[0]);
    }
    if (0 <= p->pipe[1]) {
        close(p->pipe[1]);
    }
    if (p->engine_defined) {
        OBJ_DESTRUCT(&p->engine);
    }
}
OBJ_CLASS_INSTANCE(orcm_progress_tracker_t,
                   opal_list_item_t,
                   trkcon, trkdes);

static opal_list_t tracking;
static bool inited = false;
static void wakeup(int fd, short args, void *cbdata)
{
    opal_output(0, "%s progress:wakeup invoked",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
}


opal_event_base_t *orcm_start_progress_thread(char *name,
                                              opal_thread_fn_t func)
{
    orcm_progress_tracker_t *trk;
    int rc;

    trk = OBJ_NEW(orcm_progress_tracker_t);
    trk->name = strdup(name);
    if (NULL == (trk->ev_base = opal_event_base_create())) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(trk);
        return NULL;
    }

    /* add an event it can block on */
    if (0 > pipe(trk->pipe)) {
        ORTE_ERROR_LOG(ORCM_ERR_IN_ERRNO);
        OBJ_RELEASE(trk);
        return NULL;
    }
    /* Make sure the pipe FDs are set to close-on-exec so that
       they don't leak into children */
    if (opal_fd_set_cloexec(trk->pipe[0]) != OPAL_SUCCESS ||
        opal_fd_set_cloexec(trk->pipe[1]) != OPAL_SUCCESS) {
        ORTE_ERROR_LOG(ORCM_ERR_IN_ERRNO);
        OBJ_RELEASE(trk);
        return NULL;
    }
    opal_event_set(trk->ev_base, &trk->block, trk->pipe[0], OPAL_EV_READ, wakeup, NULL);
    opal_event_add(&trk->block, 0);
    trk->block_active = true;

    /* construct the thread object */
    OBJ_CONSTRUCT(&trk->engine, opal_thread_t);
    trk->engine_defined = true;
    /* fork off a thread to progress it */
    trk->engine.t_run = func;
    if (OPAL_SUCCESS != (rc = opal_thread_start(&trk->engine))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(trk);
        return NULL;
    }
    if (!inited) {
        OBJ_CONSTRUCT(&tracking, opal_list_t);
        inited = true;
    }
    opal_list_append(&tracking, &trk->super);
    return trk->ev_base;
}

void orcm_stop_progress_thread(char *name, bool cleanup)
{
    orcm_progress_tracker_t *trk;

    if (!inited) {
        /* nothing we can do */
        return;
    }

    /* find the specified engine */
    OPAL_LIST_FOREACH(trk, &tracking, orcm_progress_tracker_t) {
        if (0 == strcmp(name, trk->name)) {
            /* break the event loop */
            opal_event_base_loopbreak(trk->ev_base);
            /* wait for thread to exit */
            opal_thread_join(&trk->engine, NULL);
            /* cleanup, if they indicated they are done with this event base */
            if (cleanup) {
                opal_list_remove_item(&tracking, &trk->super);
                OBJ_RELEASE(trk);
            }
            return;
        }
    }
}

int orcm_restart_progress_thread(char *name)
{
    orcm_progress_tracker_t *trk;
    int rc;

    if (!inited) {
        /* nothing we can do */
        return ORCM_ERROR;
    }

    /* find the specified engine */
    OPAL_LIST_FOREACH(trk, &tracking, orcm_progress_tracker_t) {
        if (0 == strcmp(name, trk->name)) {
            if (!trk->engine_defined) {
                ORTE_ERROR_LOG(ORCM_ERR_NOT_SUPPORTED);
                return ORCM_ERR_NOT_SUPPORTED;
            }
            if (OPAL_SUCCESS != (rc = opal_thread_start(&trk->engine))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    }

    return ORCM_ERR_NOT_FOUND;
}

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

#include <sys/inotify.h>

#include "opal/util/output.h"
#include "opal/mca/event/event.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/show_help.h"

#include "orcm/mca/scd/base/base.h"
#include "scd_mfile.h"

static int init(void);
static void finalize(void);


orcm_scd_base_module_t orcm_scd_mfile_module = {
    init,
    finalize
};

static void mfile_undef(int sd, short args, void *cbdata);
static void mfile_find_queue(int sd, short args, void *cbdata);
static void mfile_schedule(int sd, short args, void *cbdata);
static void mfile_allocated(int sd, short args, void *cbdata);
static void mfile_terminated(int sd, short args, void *cbdata);
static void mfile_cancel(int sd, short args, void *cbdata);

static orcm_scd_session_state_t states[] = {
    ORCM_SESSION_STATE_UNDEF,
    ORCM_SESSION_STATE_INIT,
    ORCM_SESSION_STATE_SCHEDULE,
    ORCM_SESSION_STATE_ALLOCD,
    ORCM_SESSION_STATE_TERMINATED,
    ORCM_SESSION_STATE_CANCEL
};
static orcm_scd_state_cbfunc_t callbacks[] = {
    mfile_undef,
    mfile_find_queue,
    mfile_schedule,
    mfile_allocated,
    mfile_terminated,
    mfile_cancel
};

static FILE *resourcefile;
static int notifier = -1;
static int watch;
static opal_event_t watch_ev;
static bool watch_active = false;
static void inotify_handler(int fd, short args, void *cbdata);

static int init(void)
{
    int i, rc;
    int num_states;
    char *file;
    orcm_node_t *node;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:mfile:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* define our state machine */
    num_states = sizeof(states) / sizeof(orcm_scd_session_state_t);
    for (i=0; i < num_states; i++) {
        if (ORCM_SUCCESS != (rc = orcm_scd_base_add_session_state(states[i],
                                                                    callbacks[i],
                                                                    ORTE_SYS_PRI))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    /* inform Moab about our nodes */
    (void)asprintf(&file, "%s/resources.txt", mca_scd_mfile_component.moab_interface_dir);
    resourcefile = fopen(file, "w");
    if (NULL == resourcefile) {
        orte_show_help("scd-mfile.txt", "file-open", true, file);
        free(file);
        return ORCM_ERR_NOT_FOUND;
    }
    for (i=0; i < orcm_scd_base.nodes.size; i ++) {
        if (NULL == (node = (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes, i))) {
            continue;
        }
        fprintf(resourcefile, "%s CPROCS=%u AMEMORY=100980 STATE=idle\n", node->name, node->npes);
    }
    fclose(resourcefile);

    /* init the inotifier system */
    notifier = inotify_init();

    /* setup to watch the interface dir after we loaded the nodes so we don't
     * have to service our self-created event - CREATE always is followed by
     * a MODIFY event, so don't need both */
    if (0 > (watch = inotify_add_watch(notifier, mca_scd_mfile_component.moab_interface_dir,
                                       IN_DELETE | IN_MODIFY | IN_MOVE))) {
        /* error */
        close(notifier);
        notifier = -1;
        ORTE_ERROR_LOG(ORTE_ERR_NOT_AVAILABLE);
        return ORTE_ERR_NOT_AVAILABLE;
    }
    /* start the watcher event */
    watch_active = true;
    opal_event_set(orcm_scd_base.ev_base, &watch_ev, notifier,
                   OPAL_EV_READ|OPAL_EV_PERSIST, inotify_handler, NULL);

    /* start the receive - be sure this happens *after* we define
     * the state machine in case a cmd hits right away */
    if (ORCM_SUCCESS != (rc = orcm_scd_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:mfile:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    orcm_scd_base_comm_stop();

    if (0 <= notifier) {
        close(notifier);
    }
    if (watch_active) {
        opal_event_del(&watch_ev);
        watch_active = false;
    }
}


static void mfile_undef(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    /* this isn't defined - so just report the error */
    opal_output(0, "%s UNDEF SCHEDULER STATE CALLED",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    OBJ_RELEASE(caddy);
}

static void mfile_find_queue(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    orcm_queue_t *q;

    /* cycle across the queues and select the one that best
     * fits this session request.  for MFILE, its just the 
     * default always.
     */

    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "default")) {
            caddy->session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &caddy->session->super);
            ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);

            OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                                 "%s scd:mfile:find_queue %s\n",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), q->name));
            break;
        }
    }

    OBJ_RELEASE(caddy);
}

static void mfile_schedule(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    OBJ_RELEASE(caddy);
}

static void mfile_allocated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    orcm_queue_t *q;

    OPAL_OUTPUT_VERBOSE((5, orcm_scd_base_framework.framework_output,
                         "%s scd:mfile:allocated - (session: %d) got nodelist %s\n",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), 
                         caddy->session->id,
                         caddy->session->alloc->nodes));

    /* put session on running queue */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        if (0 == strcmp(q->name, "running")) {
            caddy->session->alloc->queues = strdup(q->name);
            opal_list_append(&q->sessions, &caddy->session->super);
            break;
        }
    }

    OBJ_RELEASE(caddy);
    return;
}

static void mfile_terminated(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;

    /* set nodes to UNALLOC
    */
    OBJ_RELEASE(caddy);
}

static void mfile_cancel(int sd, short args, void *cbdata)
{
    orcm_session_caddy_t *caddy = (orcm_session_caddy_t*)cbdata;
    orcm_queue_t *q;
    orcm_session_t *session;

    /* if session is queued, find it and delete it */
    OPAL_LIST_FOREACH(q, &orcm_scd_base.queues, orcm_queue_t) {
        OPAL_LIST_FOREACH(session, &q->sessions, orcm_session_t) {
            if (session->id == caddy->session->id) {
                /* if session is running, send cancel launch command */
                if (0 == strcmp(q->name, "running")) {
                    ORCM_ACTIVATE_RM_STATE(session, ORCM_SESSION_STATE_KILL);
                } else {
                    opal_list_remove_item(&q->sessions, &session->super);
                    ORCM_ACTIVATE_SCD_STATE(caddy->session, ORCM_SESSION_STATE_SCHEDULE);
                }
                break;
            }
        }
    }

    OBJ_RELEASE(caddy);
}

static void inotify_handler(int fd, short args, void *cbdata)
{
    struct {
        int wd;
        uint32_t mask;
        uint32_t cookie;
        uint32_t len;
    } data;
    char filename[4096];
    ssize_t sz;
    struct timespec tp={0, 100000};

    sz = read(fd, &data, sizeof(data));

    if (0 < data.len) {
        if (data.len != (sz = read(fd, filename, data.len))) {
            opal_output(0, "INCOMPLETE READ");
            return;
        }

        opal_output_verbose(2, orcm_scd_base_framework.framework_output,
                             "%s Inotify Event: name %s type %d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             filename, data.mask);

        /* if the event involves a typical editor swap
         * file, ignore it
         */
        if (NULL != strstr(filename, ".swp") ||
            NULL != strchr(filename, '~')) {
            /* vi swap */
            opal_output_verbose(2, orcm_scd_base_framework.framework_output,
                                 "%s Inotify Event: Ignoring .swp file",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return;
        }
        if ('.' == filename[0]) {
            /* special file */
            opal_output_verbose(2, orcm_scd_base_framework.framework_output,
                                 "%s Inotify Event: Ignoring dot-file",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return;
        }
        if ('#' == filename[0]) {
            /* emacs auto-save */
            opal_output_verbose(2, orcm_scd_base_framework.framework_output,
                                 "%s Ignoring emacs auto-save file",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return;
        }
        /* ensure that editors have a chance to cleanly
         * exit before reading the config so we don't
         * destabilize the parser
         */
        nanosleep(&tp, NULL);
        /* process the file */
    } else if (sz < 0) {
        opal_output_verbose(2, orcm_scd_base_framework.framework_output,
                             "%s Inotify Event: READ ERROR",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)); /* EINVAL: short of space */
        return;
    } else {
        opal_output_verbose(2,orcm_scd_base_framework.framework_output,
                             "%s Inotify Event: Short read: %ld", 
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (long)sz);
    }
}

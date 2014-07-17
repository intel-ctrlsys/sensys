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

#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /*  HAVE_STDLIB_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif  /* HAVE_SYS_WAIT_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/basename.h"
#include "opal/util/cmd_line.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/base.h"
#include "opal/runtime/opal.h"
#if OPAL_ENABLE_FT_CR == 1
#include "opal/runtime/opal_cr.h"
#endif

#include "orte/util/error_strings.h"
#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/state/state.h"

#include "orcm/runtime/runtime.h"

#include "orcm/mca/scd/base/base.h"

#include "orcmapi/version.h"
#include "orcmapi/orcmapi.h"

/*
 * no commandline opts for library interface
 */
static opal_cmd_line_init_t cmd_line_opts[] = {
    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int orcmapi_init(void)
{
    int ret = 0;
    opal_cmd_line_t *cmd_line = NULL;
    pid_t child;

    /* setup to check common command line options that just report and die */
    cmd_line = OBJ_NEW(opal_cmd_line_t);
    opal_cmd_line_create(cmd_line, cmd_line_opts);
    mca_base_cmd_line_setup(cmd_line);

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(cmd_line, &environ, &environ);
    
    opal_progress_set_event_flag(OPAL_EVLOOP_ONCE);

    /* select external scheduler */
    putenv("OMPI_MCA_scd=external");

    /* init the ORCM library */
    if (ORCM_SUCCESS != (ret = orcm_init(ORCM_SCHED))) {
        fprintf(stderr, "Failed to init: error %d\n", ret);
        exit(1);
    }
    
    child = fork();

    /* start the event loop in a new thread */
    if(child >= 0) {
        if(child == 0) {
            while (orte_event_base_active) {
                opal_event_loop(orte_event_base, OPAL_EVLOOP_ONCE);
            }
            orcm_finalize();
            return ORCM_SUCCESS;
        } else {
            return ret;
        }
    } else {
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
}

void orcmapi_finalize(void)
{
    ORTE_UPDATE_EXIT_STATUS(ORTE_ERR_INTERUPTED);
    ORTE_ACTIVATE_JOB_STATE(NULL, ORTE_JOB_STATE_FORCED_EXIT);

    return;
}

int orcmapi_get_nodes(liborcm_node_t ***nodes, int *count)
{
    int i, j, num_nodes;
    orcm_node_t *orcmnode;

    num_nodes = orcm_scd_base.nodes.lowest_free;

    if (0 < num_nodes) {
        *nodes = (liborcm_node_t**)malloc(num_nodes * sizeof(liborcm_node_t*));
        if (NULL == nodes) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }

        i = 0;
        for (j = 0; j < num_nodes; j++) {
            if (NULL == (orcmnode = 
                         (orcm_node_t*)opal_pointer_array_get_item(&orcm_scd_base.nodes,
                                                                   j))) {
                continue;
            }
            (*nodes)[i] = (liborcm_node_t*)malloc(sizeof(liborcm_node_t*));
            (*nodes)[i]->name = strdup(orcmnode->name);
            (*nodes)[i]->state = orcmnode->state;
            i++;
        }
    }

    return ORCM_SUCCESS;
}

int orcmapi_launch_session(int min_nodes, char *nodes, int64_t id, uid_t uid, gid_t gid)
{
    orcm_session_t *session;
    orcm_alloc_t *alloc;

    session = OBJ_NEW(orcm_session_t);
    session->alloc = OBJ_NEW(orcm_alloc_t);
    alloc = session->alloc;

    alloc->min_nodes = min_nodes;
    orte_regex_create(nodes, &(alloc->nodes));
    alloc->id = id;
    session->id = id;
    alloc->uid = uid;
    alloc->gid = gid;
    alloc->caller_uid = uid;
    alloc->caller_gid = gid;
    alloc->interactive = true;
    
    external_launch(session);
}

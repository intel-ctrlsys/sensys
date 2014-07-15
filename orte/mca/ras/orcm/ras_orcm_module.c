/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "opal/util/argv.h"
#include "opal/util/output.h"

#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rmaps/base/base.h"
#include "orte/mca/state/state.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/ras/base/ras_private.h"
#include "ras_orcm.h"

/*
 * API functions
 */
static int init(void);
static int orte_ras_orcm_allocate(orte_job_t *jdata, opal_list_t *nodes);
static void deallocate(orte_job_t *jdata, orte_app_context_t *app);
static int orte_ras_orcm_finalize(void);

/*
 * RAS orcm module
 */
orte_ras_base_module_t orte_ras_orcm_module = {
    init,
    orte_ras_orcm_allocate,
    deallocate,
    orte_ras_orcm_finalize
};

/* Local functions */
static int orte_ras_orcm_discover(char *regexp, char* slots_per_node, 
                                  opal_list_t *nodelist);


/* init the module */
static int init(void)
{
    return ORTE_SUCCESS;
}

/**
 * Discover available (pre-allocated) nodes.  Allocate the
 * requested number of nodes/process slots to the job.
 *
 */
static int orte_ras_orcm_allocate(orte_job_t *jdata, opal_list_t *nodes)
{
    int ret;
    char *orcm_node_str, *regexp, *node_slots;
    char *orcm_sessionid;

    if (NULL == (orcm_sessionid = getenv("ORCM_SESSIONID"))) {
        opal_output_verbose(2, orte_ras_base_framework.framework_output,
                            "%s ras:orcm: no prior allocation",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return ORTE_ERR_TAKE_NEXT_OPTION;
    } else {
        /* save this value in the global job ident string for
         * later use in any error reporting
         */
        orte_job_ident = strdup(orcm_sessionid);
    }

    orcm_node_str = getenv("ORCM_NODELIST");
    if (NULL == orcm_node_str) {
        orte_show_help("help-ras-orcm.txt", "orcm-env-var-not-found", 1,
                       "ORCM_NODELIST");
        return ORTE_ERR_NOT_FOUND;
    }
    regexp = strdup(orcm_node_str);

    if(NULL == regexp) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return ORTE_ERR_OUT_OF_RESOURCE;
    }

    /* get the number of process slots we were assigned on each node */
    node_slots = getenv("ORCM_SLOTS_PER_NODE");
    if (NULL == node_slots) {
        orte_show_help("help-ras-orcm.txt", "orcm-env-var-not-found", 1,
                       "ORCM_SLOTS_PER_NODE");
        return ORTE_ERR_NOT_FOUND;
    }

    ret = orte_ras_orcm_discover(regexp, node_slots, nodes);
    free(regexp);
    if (ORTE_SUCCESS != ret) {
        OPAL_OUTPUT_VERBOSE((1, orte_ras_base_framework.framework_output,
                             "%s ras:orcm:allocate: discover failed!",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        return ret;
    }
    /* record the number of allocated nodes */
    orte_num_allocated_nodes = opal_list_get_size(nodes);

    /* All done */

    OPAL_OUTPUT_VERBOSE((1, orte_ras_base_framework.framework_output,
                         "%s ras:orcm:allocate: success",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORTE_SUCCESS;
}

static void deallocate(orte_job_t *jdata,
                       orte_app_context_t *app)
{
}

static int orte_ras_orcm_finalize(void)
{
    return ORTE_SUCCESS;
}


/**
 * Discover the available resources.
 *
 * @param *regexp A node regular expression from orcm (i.e. ORCM_NODELIST)
 * @param *slots_per_node A tasks per node expression from ORCM
 *                        (i.e. ORCM_SLOTS_PER_NODE)
 * @param *nodelist A list which has already been constucted to return
 *                  the found nodes in
 */
static int orte_ras_orcm_discover(char *regexp, char *slots_per_node, 
                                  opal_list_t* nodelist)
{
    int i, ret, num_nodes;
    char **names = NULL;
    int *slots;
    int slot;

    OPAL_OUTPUT_VERBOSE((1, orte_ras_base_framework.framework_output,
                         "%s ras:orcm:allocate:discover: checking nodelist: %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         regexp));

    if (ORTE_SUCCESS != (ret = orte_regex_extract_node_names(regexp, &names))) {
        OPAL_OUTPUT_VERBOSE((5, orte_ras_base_framework.framework_output,
                             "%s ras:orcm:allocate:discover: regex returned null nodelist",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        if (NULL != names) {
            opal_argv_free(names);
        }
        return ret;
    }
    if (NULL == names) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return ORTE_ERR_OUT_OF_RESOURCE;
    }

    num_nodes = opal_argv_count(names);

    /* Find the number of slots per node */

    slots = malloc(sizeof(int) * num_nodes);
    if (NULL == slots) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return ORTE_ERR_OUT_OF_RESOURCE;
    }

    /* for now, just assign the slots/nodes to each node
     * but we need to do something smarter when this
     * really gets imlemented in ORCM
     */
    slot = atoi(slots_per_node);
    if (0 > slot) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
    for (i = 0; i < num_nodes; i++) {
        slots[i] = slot;
    }

    /* Convert the argv of node names to a list of node_t's */

    for (i = 0; NULL != names && NULL != names[i]; ++i) {
        orte_node_t *node;

        OPAL_OUTPUT_VERBOSE((1, orte_ras_base_framework.framework_output,
                             "%s ras:orcm:allocate:discover: adding node %s (%d slot%s)",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             names[i], slots[i], (1 == slots[i]) ? "" : "s"));

        node = OBJ_NEW(orte_node_t);
        if (NULL == node) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            free(slots);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        node->name = strdup(names[i]);
        node->state = ORTE_NODE_STATE_UP;
        node->slots_inuse = 0;
        node->slots_max = 0;
        node->slots = slots[i];
        opal_list_append(nodelist, &node->super);
    }
    free(slots);
    opal_argv_free(names);

    /* All done */
    return ret;
}


/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_RM_TYPES_H
#define MCA_RM_TYPES_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* for time_t */
#endif

#include "opal/class/opal_object.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"


BEGIN_C_DECLS

/****    NODESTATE TYPE    ****/
typedef int8_t orcm_node_state_t;
#define ORCM_NODE_STATE_T OPAL_INT8

#define ORCM_NODE_STATE_UNDEF         0  // Node is undefined
#define ORCM_NODE_STATE_UNKNOWN       1  // Node is in unknown state
#define ORCM_NODE_STATE_UP            2  // Node is up
#define ORCM_NODE_STATE_DOWN          3  // Node is down
#define ORCM_NODE_STATE_SESTERM       4  // Node is terminating session


/****    SESSION STATES GOING THROUGH RM    ****/
typedef uint32_t orcm_rm_session_state_t;
#define ORCM_SESSION_STATE_UNDEF         0  // Session state is undefined
#define ORCM_SESSION_STATE_REQ           1  // Session requesting resources

/****    STATE MACHINE    ****/
typedef void (*orcm_rm_state_cbfunc_t)(int fd, short args, void* cb);

typedef struct {
    opal_list_item_t super;
    orcm_rm_session_state_t state;
    orcm_rm_state_cbfunc_t cbfunc;
    int priority;
} orcm_rm_state_t;
ORTE_DECLSPEC OBJ_CLASS_DECLARATION(orcm_rm_state_t);

/* define a few commands */
typedef uint8_t orcm_rm_cmd_flag_t;
#define ORCM_RM_CMD_T OPAL_UINT8

#define ORCM_NODESTATE_REQ_COMMAND    1
#define ORCM_RESOURCE_REQ_COMMAND     2
#define ORCM_NODESTATE_UPDATE_COMMAND 3


END_C_DECLS

#endif

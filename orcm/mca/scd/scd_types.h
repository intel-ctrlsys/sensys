/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_SCD_TYPES_H
#define MCA_SCD_TYPES_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* for time_t */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h> /* for uid_t, gid_t */
#endif

#include "opal/class/opal_object.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"

BEGIN_C_DECLS

/* Provide an enum of resource types for use
 * in specifying constraints
 */
typedef enum {
    ORCM_RESOURCE_MEMORY,
    ORCM_RESOURCE_CPU,
    ORCM_RESOURCE_BANDWIDTH,
    ORCM_RESOURCE_IMAGE
} orcm_resource_type_t;

/* Describe a resource constraint to be applied
 * when selecting nodes for an allocation. Includes
 * memory, network QoS, and OS image.
 */
typedef struct {
    opal_list_item_t super;
    orcm_resource_type_t type;
    char *constraint;
} orcm_resource_t;
OBJ_CLASS_DECLARATION(orcm_resource_t);

/****    ALLOCATION TYPES    ****/
typedef int64_t orcm_alloc_id_t;
#define ORCM_ALLOC_ID_T OPAL_INT64

typedef struct {
    opal_object_t super;
    orcm_alloc_id_t id;       // unique id
    int32_t priority;         // session priority
    char *account;            // account to be charged
    char *name;               // user-assigned project name
    int32_t gid;              // group id to be run under
    int32_t max_nodes;        // max number of nodes
    int32_t max_pes;          // max number of processing elements
    int32_t min_nodes;        // min number of nodes required
    int32_t min_pes;          // min number of pe's required
    time_t begin;             // desired start time for allocation
    time_t walltime;          // max execution time
    bool exclusive;           // true if nodes to be exclusively allocated (i.e., not shared across sessions)
    uid_t caller_uid;         // uid of submission request
    gid_t caller_gid;         // gid of submission request
    bool interactive;         // interactive or batch
    orte_process_name_t hnp;  // hnp process name
    char *hnpname;            // hnp string name
    char *nodefile;           // file listing names and/or regex of candidate nodes to be used
    char *nodes;              // regex of nodes to be used
    char *queues;             // comma-delimited list of queue names
    opal_list_t constraints;  // list of resource constraints to be applied when selecting hosts
} orcm_alloc_t;
OBJ_CLASS_DECLARATION(orcm_alloc_t);

/****    QUEUE TYPE    ****/
/* ORCM maintains a collection of queues for holding
 * pending session requests. The configuration of
 * queues (e.g., how many and their relative priority)
 * is provided at startup from the orcm-site.xml
 * configuration file. Addition and deletion of queues
 * can be done dynamically via a console command.
 * Individual nodes can only be assigned to one queue
 * at a time, but can be moved across queues on-the-fly.
 * Changing queues will not impact executing applications
 *
 * Each queue maintains a priority-ordered array of
 * pending session requests, organized in "bins"
 * according to the range of various resources. For
 * example, a queue may contain a "power_binned" where
 * each element in the array contains session requests
 * that require power within a certain range. Thus,
 * there might be an array of session requests whose
 * power requirement falls between 100-200kw, and another
 * array of requests whose power requirement falls
 * in the 200-300kw range. This allows the scheduler
 * to rapidly select a session to be started without
 * having to consider all options in a linear manner.
 *
 * Note that a single session request could appear
 * in multiple binned arrays - e.g., once for power
 * and again for nodes.
 */
typedef struct {
    opal_list_item_t super;
    char *name;
    int32_t priority;
    opal_list_t sessions;
} orcm_queue_t;
OBJ_CLASS_DECLARATION(orcm_queue_t);


/****    NODESTATE TYPE    ****/
typedef int8_t orcm_scd_node_state_t;
#define ORCM_SCD_NODE_STATE_T OPAL_INT8

#define ORCM_SCD_NODE_STATE_UNDEF      0  // Node is undefined
#define ORCM_SCD_NODE_STATE_UNKNOWN    1  // Node is unknown
#define ORCM_SCD_NODE_STATE_UNALLOC    2  // Node is unallocated
#define ORCM_SCD_NODE_STATE_ALLOC      3  // Node is allocated
#define ORCM_SCD_NODE_STATE_EXCLUSIVE  4  // Node is exclusively allocated


/****    JOB TYPE    ****/
/* The ORCM scheduler doesn't need to track the detailed
 * information found in the ORTE job object as the scheduler
 * isn't tasked with actually executing the job - e.g., it doesn't
 * track the status of individual processes. In the interest
 * of saving memory footprint, we therefore define a limited
 * job object that only contains the info required by the
 * scheduler
 */
typedef struct {
    opal_object_t super;
} orcm_job_t;
OBJ_CLASS_DECLARATION(orcm_job_t);


/****    STEP TYPE    ****/
/* An ORCM "step" consists of a single execution element, comprised
 * of a set of allocated nodes combined with a specified application
 * to be executed. The application may be comprised of multiple binaries
 * to be executed in parallel - i.e., in a MIMD fashion.
 */
typedef struct {
    opal_list_item_t super;
    orcm_alloc_t *alloc;         // requested allocation
    orcm_job_t *job;             // application to be executed
    opal_pointer_array_t nodes;  // actual nodes allocated
} orcm_step_t;
OBJ_CLASS_DECLARATION(orcm_step_t);

/****    SESSION TYPE    ****/
/* ORCM sessions are comprised of one or more "steps", each step
 * comprised of an allocated set of nodes executing an application.
 * Multiple steps can be executing in parallel - e.g., by the
 * user executing "orun ..." commands in the background. A session
 * represents one submission by the user - i.e., either issuing an
 * "obatch" command that contains an execution script, or issuing
 * an "orun" command from outside an existing allocation.
 */
typedef uint32_t orcm_scd_session_state_t;
typedef uint32_t orcm_session_id_t;
typedef struct {
    opal_list_item_t super;
    orcm_session_id_t id;
    int32_t uid;
    orte_process_name_t requestor;
    orcm_alloc_t *alloc;  // master allocation for the session
    opal_list_t steps;
} orcm_session_t;
OBJ_CLASS_DECLARATION(orcm_session_t);


/* SESSION STATES */
#define ORCM_SESSION_STATE_UNDEF          0
#define ORCM_SESSION_STATE_INIT           1 // not yet assigned to a queue
#define ORCM_SESSION_STATE_SCHEDULE       2 // run schedulers
#define ORCM_SESSION_STATE_ALLOCD         3 // allocated, job not started
#define ORCM_SESSION_STATE_ACTIVE         4 // job step(s) running
#define ORCM_SESSION_STATE_TERMINATED     5 // allocation terminated

#define ORCM_SESSION_STATE_ANY           10 // marker

#define ORCM_SESSION_STATE_ERROR         20 // marker


/****    STATE MACHINE    ****/
typedef void (*orcm_scd_state_cbfunc_t)(int fd, short args, void* cb);

typedef struct {
    opal_list_item_t super;
    orcm_scd_session_state_t state;
    orcm_scd_state_cbfunc_t cbfunc;
    int priority;
} orcm_scd_state_t;
ORTE_DECLSPEC OBJ_CLASS_DECLARATION(orcm_scd_state_t);

typedef struct {
    opal_object_t super;
    opal_event_t ev;
    orcm_session_t *session;
} orcm_session_caddy_t;
OBJ_CLASS_DECLARATION(orcm_session_caddy_t);


/* define a few commands */
typedef uint8_t orcm_scd_cmd_flag_t;
#define ORCM_SCD_CMD_T OPAL_UINT8

#define ORCM_SESSION_REQ_COMMAND   1
#define ORCM_SESSION_INFO_COMMAND  2
#define ORCM_NODE_INFO_COMMAND     3
#define ORCM_RUN_COMMAND           4
#define ORCM_VM_READY_COMMAND      5


END_C_DECLS

#endif

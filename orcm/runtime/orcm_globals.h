/*
 * Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/**
 * @file
 *
 * Interface into the ORCM Library
 */
#ifndef ORCM_GLOBALS_H
#define ORCM_GLOBALS_H

#include "orcm_config.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "opal/mca/event/event.h"
#include "opal/class/opal_list.h"

#include "orte/util/proc_info.h"

BEGIN_C_DECLS

/* define some process types */
typedef orte_proc_type_t orcm_proc_type_t;
#define ORCM_TOOL           (ORTE_PROC_TOOL | ORTE_PROC_CM)
#define ORCM_DAEMON         (ORTE_PROC_DAEMON | ORTE_PROC_CM)
#define ORCM_AGGREGATOR     (ORTE_PROC_AGGREGATOR | ORTE_PROC_CM)
#define ORCM_IOF_ENDPT      (ORTE_PROC_IOF_ENDPT | ORTE_PROC_CM)
#define ORCM_SCHED          (ORTE_PROC_SCHEDULER | ORTE_PROC_CM)
#define ORCM_MASTER         (ORTE_PROC_MASTER | ORTE_PROC_CM)
#define ORCM_EMULATOR       (ORTE_PROC_EMULATOR | ORTE_PROC_CM)

/* just define these tests to their ORTE equivalent
 * as otherwise they will always be true since anything
 * in ORCM will have ORTE_PROC_CM set
 */
#define ORCM_PROC_IS_DAEMON      ORTE_PROC_IS_DAEMON
#define ORCM_PROC_IS_AGGREGATOR  ORTE_PROC_IS_AGGREGATOR
#define ORCM_PROC_IS_SCHED       ORTE_PROC_IS_SCHEDULER
#define ORCM_PROC_IS_TOOL        ORTE_PROC_IS_TOOL
#define ORCM_PROC_IS_IOF_ENDPT   ORTE_PROC_IS_IOF_ENDPT
#define ORCM_PROC_IS_MASTER      ORTE_PROC_IS_MASTER
#define ORCM_PROC_IS_EMULATOR    ORTE_PROC_IS_EMULATOR

/****    NODESTATE TYPE    ****/
typedef int8_t orcm_node_state_t;
#define ORCM_NODE_STATE_T OPAL_INT8

#define ORCM_NODE_STATE_UNDEF         0  // Node is undefined
#define ORCM_NODE_STATE_UNKNOWN       1  // Node is in unknown state
#define ORCM_NODE_STATE_UP            2  // Node is up
#define ORCM_NODE_STATE_DOWN          3  // Node is down
#define ORCM_NODE_STATE_SESTERM       4  // Node is terminating session

/* define a few commands for sending between orcmd's and orcmsched */
typedef uint8_t orcm_rm_cmd_flag_t;
#define ORCM_RM_CMD_T OPAL_UINT8

#define ORCM_NODESTATE_REQ_COMMAND    1
#define ORCM_RESOURCE_REQ_COMMAND     2
#define ORCM_NODESTATE_UPDATE_COMMAND 3
#define ORCM_VM_READY_COMMAND         4
#define ORCM_LAUNCH_STEPD_COMMAND     5
#define ORCM_CANCEL_STEPD_COMMAND     6
#define ORCM_STEPD_COMPLETE_COMMAND   7
#define ORCM_CALIBRATE                8

/** version string of ORCM */
ORCM_DECLSPEC extern const char openrcm_version_string[];

/**
 * Whether ORCM is initialized or we are in openrcm_finalize
 */
ORCM_DECLSPEC extern int orcm_initialized;
ORCM_DECLSPEC extern bool orcm_finalizing;

/* debugger output control */
ORCM_DECLSPEC extern int orcm_debug_output;
ORCM_DECLSPEC extern int orcm_debug_verbosity;


/* extend the ORTE RML tags to add our own */
#define ORCM_RML_TAG_SCD   (ORTE_RML_TAG_MAX + 1)
#define ORCM_RML_TAG_RM    (ORTE_RML_TAG_MAX + 2)


/* define event base priorities */
#define ORCM_SCHED_PRI OPAL_EV_MSG_HI_PRI
#define ORCM_INFO_PRI  OPAL_EV_INFO_HI_PRI


/* system descriptions */
ORCM_DECLSPEC extern opal_list_t *orcm_clusters;
ORCM_DECLSPEC extern opal_list_t *orcm_schedulers;

/**
 * Init the ORCM datatype support
 */
ORCM_DECLSPEC int orcm_dt_init(void);

const char *orcm_node_state_to_str(orcm_node_state_t state)

END_C_DECLS

#endif /* ORCM_GLOBALS_H */

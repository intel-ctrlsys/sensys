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

#include "orcm/mca/cfgi/cfgi_types.h"

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

typedef struct {
    opal_list_item_t super;
    orte_node_state_t state;
} orcm_node_state_t;
ORTE_DECLSPEC OBJ_CLASS_DECLARATION(orcm_node_state_t);

typedef struct {
    opal_object_t super;
    opal_event_t ev;
    orcm_resource_t *resource;
} orcm_resource_caddy_t;
OBJ_CLASS_DECLARATION(orcm_resource_caddy_t);

/* define a few commands */
typedef uint8_t orcm_rm_cmd_flag_t;
#define ORCM_RM_CMD_T OPAL_UINT8

#define ORCM_NODESTATE_REQ_COMMAND   1
#define ORCM_RESOURCE_REQ_COMMAND    2


END_C_DECLS

#endif

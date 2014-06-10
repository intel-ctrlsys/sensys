/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_SCD_BASE_H
#define MCA_SCD_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/scd/scd.h"
#include "orcm/mca/cfgi/cfgi_types.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_scd_base_framework;
/*
 * Select an available component.
 */
ORCM_DECLSPEC int orcm_scd_base_select(void);

typedef struct {
    opal_object_t super;
    opal_event_t ev;
    orcm_scheduler_t *scheduler;
} orcm_scheduler_caddy_t;
OBJ_CLASS_DECLARATION(orcm_scheduler_caddy_t);

#define ORCM_CONSTRUCT_QUEUES(a)                                         \
    do {                                                                 \
        orcm_scheduler_caddy_t *s;                                       \
        opal_output_verbose(1, orcm_scd_base_framework.framework_output, \
                            "%s CONSTRUCT QUEUES AT %s:%d",              \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),          \
                            __FILE__, __LINE__);                         \
        s = OBJ_NEW(orcm_scheduler_caddy_t);                             \
        s->scheduler = (a);                                              \
        opal_event_set(orcm_scd_base.ev_base, &s->ev, -1, OPAL_EV_WRITE, \
                       orcm_scd_base_construct_queues, s);               \
        opal_event_set_priority(&s->ev, ORCM_SCHED_PRI);                 \
        opal_event_active(&s->ev, OPAL_EV_WRITE, 1);                     \
    } while(0);

typedef struct {
    /* flag that we just want to test */
    bool test_mode;
    /* define an event base strictly for scheduling - this
     * allows the scheduler to respond to requests for
     * information without interference with the
     * actual scheduling computation
     */
    opal_event_base_t *ev_base;
    bool ev_active;
    /* scd state machine */
    opal_list_t states;
    /* rm state machine */
    opal_list_t rmstates;
    /* selected plugin */
    orcm_scd_base_module_t *module;
    /* queues for tracking session requests */
    opal_list_t queues;
    /* node tracking */
    opal_pointer_array_t nodes;
} orcm_scd_base_t;
ORCM_DECLSPEC extern orcm_scd_base_t orcm_scd_base;

/* start/stop base receive */
ORCM_DECLSPEC int orcm_scd_base_comm_start(void);
ORCM_DECLSPEC int orcm_scd_base_comm_stop(void);

/* base code stubs */
ORCM_DECLSPEC void orcm_scd_base_activate_session_state(orcm_session_t *s,
                                                        orcm_scd_session_state_t state);

/* datatype support */
ORCM_DECLSPEC int orcm_pack_alloc(opal_buffer_t *buffer, const void *src,
                                  int32_t num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_unpack_alloc(opal_buffer_t *buffer, void *dest,
                                    int32_t *num_vals, opal_data_type_t type);
ORCM_DECLSPEC int orcm_compare_alloc(orcm_alloc_t *value1,
                                     orcm_alloc_t *value2,
                                     opal_data_type_t type);
ORCM_DECLSPEC int orcm_copy_alloc(orcm_alloc_t **dest,
                                  orcm_alloc_t *src,
                                  opal_data_type_t type);
ORCM_DECLSPEC int orcm_print_alloc(char **output, char *prefix,
                                   orcm_alloc_t *src, opal_data_type_t type);

ORCM_DECLSPEC const char *orcm_scd_session_state_to_str(orcm_scd_session_state_t state);
ORCM_DECLSPEC const char *orcm_scd_node_state_to_str(orcm_scd_node_state_t state);
ORCM_DECLSPEC int orcm_scd_base_add_session_state(orcm_scd_session_state_t state,
                                                  orcm_scd_state_cbfunc_t cbfunc,
                                                  int priority);
ORCM_DECLSPEC void orcm_scd_base_construct_queues(int fd, short args, void *cbdata);
ORCM_DECLSPEC int orcm_scd_base_get_next_session_id(void);

END_C_DECLS
#endif

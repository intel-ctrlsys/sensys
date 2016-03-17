/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015-2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * @file:
 *
 */

#ifndef MCA_DISPATCH_H
#define MCA_DISPATCH_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/types.h"

#include "orcm/mca/mca.h"

#include "orte/types.h"

#include "orcm/mca/dispatch/dispatch_types.h"

BEGIN_C_DECLS

/* expose the base verbose output for use in macros */
extern int orcm_dispatch_base_output;

/* expose the event base for this framework */
extern opal_event_base_t *orcm_dispatch_evbase;

/* expose the base function that will process the
 * event declarations */
extern void orcm_dispatch_base_event(int sd, short args, void *cbdata);

/* define a macro to be used for declaring a RAS event */
#define ORCM_RAS_EVENT(a)                                                            \
    do {                                                                             \
        opal_output_verbose(1, orcm_dispatch_base_output,                       \
                            "%s[%s:%d] RAS EVENT: %s:%s",                            \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),                      \
                            __FILE__, __LINE__,                                      \
                            orcm_dispatch_base_print_type((a)->type),           \
                            orcm_dispatch_base_print_severity((a)->severity));  \
        opal_event_set(orcm_dispatch_evbase, &((a)->ev), -1,                    \
                       OPAL_EV_WRITE,                                                \
                       orcm_dispatch_base_event, (a));                          \
        opal_event_active((&(a)->ev), OPAL_EV_WRITE, 1);                             \
    } while(0);

/* provide a convenience macro for adding reporter info
 * to an event. Input values:
 *
 * ev = pointer to the event (*orcm_ras_event_t)
 * k  = the reporter key to be added
 * d  = pointer to the data
 * t  = OPAL data type of the data
 */
#define ORCM_RAS_REPORTER(ev, k, d, t)                      \
    do {                                                    \
        opal_value_t *_v;                                   \
        _v = OBJ_NEW(opal_value_t);                         \
        (_v)->key = strdup((k));                            \
        opal_value_load(_v, (d), (t));                      \
        opal_list_append(&(ev)->reporter, &(_v)->super);    \
    } while(0);

/* provide a convenience macro for adding description info
 * to an event. Input values:
 *
 * ev = pointer to the event (*orcm_ras_event_t)
 * k  = the description key to be added
 * d  = pointer to the data
 * t  = OPAL data type of the data
 */
#define ORCM_RAS_DESCRIPTION(ev, k, d, t)                       \
    do {                                                        \
        opal_value_t *_v;                                       \
        _v = OBJ_NEW(opal_value_t);                             \
        (_v)->key = strdup((k));                                \
        opal_value_load(_v, (d), (t));                          \
        opal_list_append(&(ev)->description, &(_v)->super);     \
    } while(0);

/* provide a convenience macro for adding data
 * to an event. Input values:
 *
 * ev = pointer to the event (*orcm_ras_event_t)
 * k  = the description key to be added
 * d  = pointer to the data (can be an array object)
 * t  = OPAL data type of the data
 */
#define ORCM_RAS_DATA(ev, k, d, t)                      \
    do {                                                \
        opal_value_t *_v;                               \
        _v = OBJ_NEW(opal_value_t);                     \
        (_v)->key = strdup((k));                        \
        opal_value_load(_v, (d), (t));                  \
        opal_list_append(&(ev)->data, &(_v)->super);    \
    } while(0);

/*
 * Component functions - all MUST be provided! These will never be
 * called directly, but are always accessed via the ORCM_RAS_EVENT
 * macro defined above
 */

/* initialize the module */
typedef void (*orcm_dispatch_module_init_fn_t)(void);

/* finalize the module */
typedef void (*orcm_dispatch_module_fini_fn_t)(void);

/* generate a RAS event in the module's format - the module
 * is allowed to ignore the event if the incoming alert
 * is not supported in this module */
typedef void (*orcm_dispatch_module_generate_fn_t)(orcm_ras_event_t *cd);

/*
 * Ver 1.0
 */
struct orcm_dispatch_base_module_1_0_0_t {
    orcm_dispatch_module_init_fn_t      init;
    orcm_dispatch_module_fini_fn_t      finalize;
    orcm_dispatch_module_generate_fn_t  generate;
};

typedef struct orcm_dispatch_base_module_1_0_0_t orcm_dispatch_base_module_1_0_0_t;
typedef orcm_dispatch_base_module_1_0_0_t orcm_dispatch_base_module_t;

/*
 * the standard component data structure
 */
struct orcm_dispatch_base_component_1_0_0_t {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
    char *data_measured;
};
typedef struct orcm_dispatch_base_component_1_0_0_t orcm_dispatch_base_component_1_0_0_t;
typedef orcm_dispatch_base_component_1_0_0_t orcm_dispatch_base_component_t;

/*
 * Macro for use in components that are of type dispatch v1.0.0
 */
#define ORCM_DISPATCH_BASE_VERSION_1_0_0 \
  /* evgen v1.0 is chained to MCA v2.0 */ \
    ORCM_MCA_BASE_VERSION_2_1_0("dispatch", 1, 0, 0)


END_C_DECLS

#endif /* MCA_DISPATCH_H */

/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef COTT_MOCKING_H
#define COTT_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <memory.h>
    #include "opal/class/opal_hash_table.h"
    #include "opal/mca/event/libevent2022/libevent/include/event2/event.h"
    #include "orcm/mca/analytics/base/analytics_private.h"

    extern orcm_analytics_value_t* orcm_util_load_orcm_analytics_value(opal_list_t *key, opal_list_t *non_compute,
                                                                       opal_list_t *compute);

    extern void* __real_malloc(size_t size);
    extern int __real_opal_hash_table_init(opal_hash_table_t* ht, size_t table_size);
    extern int __real_event_assign(struct event *, struct event_base *, evutil_socket_t, short, event_callback_fn, void *);
    extern void __real_event_active(struct event *ev, int res, short ncalls);
    extern void __real_orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t*,
                                                                            orcm_workflow_step_t*,
                                                                            uint64_t,
                                                                            orcm_analytics_value_t*);
#ifdef __cplusplus
}
#endif // __cplusplus

typedef void* (*malloc_callback_fn_t)(size_t size);
typedef int (*opal_hash_table_init_callback_fn_t)(opal_hash_table_t* ht, size_t table_size);
typedef int (*event_assign_fn_t)(struct event*, struct event_base*, evutil_socket_t, short, event_callback_fn, void*);
typedef void (*event_active_fn_t)(struct event*, int, short);
typedef void (*analytics_base_activate_analytics_workflow_step_fn_t)(orcm_workflow_t*,
                                                                     orcm_workflow_step_t*,
                                                                     uint64_t,
                                                                     orcm_analytics_value_t*);
class cott_tests_mocking
{
    public:
        cott_tests_mocking();

        // Public Callbacks
        malloc_callback_fn_t malloc_callback;
        opal_hash_table_init_callback_fn_t opal_hash_table_init_callback;
        event_assign_fn_t event_assign_callback;
        event_active_fn_t event_active_callback;
        analytics_base_activate_analytics_workflow_step_fn_t analytics_base_activate_analytics_workflow_step_callback;
};

extern cott_tests_mocking cott_mocking;

#endif // COTT_MOCKING_H

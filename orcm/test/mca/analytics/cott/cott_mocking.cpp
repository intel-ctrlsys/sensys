/*
 * Copyright (c) 2015  Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "cott_mocking.h"

using namespace std;

cott_tests_mocking cott_mocking;

extern "C" { // Mocking must use correct "C" linkages
    void* __wrap_malloc(size_t size)
    {
        if(NULL == cott_mocking.malloc_callback) {
            return __real_malloc(size);
        } else {
            return cott_mocking.malloc_callback(size);
        }
    }

    int __wrap_opal_hash_table_init(opal_hash_table_t* ht, size_t table_size) {
        if(NULL == cott_mocking.opal_hash_table_init_callback) {
            return __real_opal_hash_table_init(ht, table_size);
        } else {
            return cott_mocking.opal_hash_table_init_callback(ht, table_size);
        }
    }

    int __wrap_event_assign(struct event*, struct event_base*, evutil_socket_t, short, event_callback_fn, void*)
    {
        return 0;
    }

    void __wrap_event_active(struct event*, int, short)
    {
    }

    void __wrap_orcm_analytics_base_activate_analytics_workflow_step(orcm_workflow_t*,
                                                                     orcm_workflow_step_t*,
                                                                     uint64_t,
                                                                     orcm_analytics_value_t*)
    {
    }
} // extern "C"

cott_tests_mocking::cott_tests_mocking()
    : malloc_callback(NULL), opal_hash_table_init_callback(NULL),
      event_assign_callback(NULL), event_active_callback(NULL),
      analytics_base_activate_analytics_workflow_step_callback(NULL)
{
}

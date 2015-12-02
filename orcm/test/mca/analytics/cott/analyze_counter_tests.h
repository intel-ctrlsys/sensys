/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ANALYZE_COUNTER_TESTS_H
#define ANALYZE_COUNTER_TESTS_H

// C
#include <stdint.h>
#include <time.h>

// C++
#include <vector>
#include <queue>

#include "gtest/gtest.h"
#include "cott_mocking.h"

typedef void (*activate_next_wf_fn_t)(orcm_workflow_caddy_t* caddy);
extern activate_next_wf_fn_t activate_next_wf_hook;
typedef int (*assert_caddy_data_fn_t)(void *cbdata);
extern assert_caddy_data_fn_t assert_caddy_data_hook;

class analyze_counter_tests : public testing::Test
{
    protected: // gtest required methods
        static void SetUpTestCase();
        static void TearDownTestCase();
        static void ResetTestCase();

        // Callbacks
        static void RasEventFired(uint32_t count, time_t elapsed, const std::vector<uint32_t>& data, void* cb_data);
        static void RasHostEventFired(const std::string& host, const std::string& label, uint32_t count, time_t elapsed,
                                      const std::vector<uint32_t>& data, void* cb_data);

        // Mocking
        static void* Malloc(size_t size);
        static int OpalHashTableInit(opal_hash_table_t* ht, size_t table_size);
        static void ActivateNextWF(orcm_workflow_caddy_t* caddy);
        static int AssertCaddyData(void* caddy);

        // Helper Methods
        static orcm_workflow_caddy_t* MakeCaddy(bool include_hostname = true,
                                                bool include_ts = true,
                                                bool include_large_count = true,
                                                const char* str_threshold = "100",
                                                const char* store = "yes",
                                                const char* action = "none",
                                                const char* severity = "error",
                                                const char* fault = "hard");
        static void DestroyCaddy(orcm_workflow_caddy_t*& caddy);

        // Data
        static uint32_t last_event_count_;
        static time_t last_event_elapsed_;
        static std::vector<uint32_t> last_event_raw_data_;
        static std::string last_event_host_;
        static std::string last_event_label_;
        static uint32_t last_callback_count_;
        static std::queue<bool> fail_malloc_call_;
        static assert_caddy_data_fn_t saved_assert_caddy_data_hook_;
};

#endif // ANALYZE_COUNTER_TESTS_H

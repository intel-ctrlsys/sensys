/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef GREI_ORCM_TEST_MCA_ANALYTICS_GENEX_ANALYTICS_GENEX_TEST_H_
#define GREI_ORCM_TEST_MCA_ANALYTICS_GENEX_ANALYTICS_GENEX_TEST_H_

extern "C" {
    #include "orcm/mca/analytics/genex/analytics_genex.h"

    extern int monitor_genex(genex_workflow_value_t *workflow_value, void* cbdata, opal_list_t* event_list);
    extern int get_genex_policy(void *cbdata, genex_workflow_value_t *workflow_value);
    extern void dest_genex_workflow_value(genex_workflow_value_t *workflow_value, orcm_workflow_caddy_t *genex_analyze_caddy);

}

#endif /* GREI_ORCM_TEST_MCA_ANALYTICS_GENEX_ANALYTICS_GENEX_TEST_H_ */

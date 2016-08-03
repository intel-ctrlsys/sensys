/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_MCA_ANALYTICS_BASE_C_ANALYTICS_FACTORY_H_
#define ORCM_MCA_ANALYTICS_BASE_C_ANALYTICS_FACTORY_H_

#ifdef __cplusplus
extern "C" {
#endif
    int search_plugin_creator(const char *directory, const char *plugin_name);
    bool check_plugin_exist(const char *plugin_name);
#ifdef __cplusplus
}
#endif


#endif /* ORCM_MCA_ANALYTICS_BASE_C_ANALYTICS_FACTORY_H_ */

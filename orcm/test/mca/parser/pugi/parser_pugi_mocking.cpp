/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "parser_pugi_mocking.h"

#include <iostream>
using namespace std;

bool is_orcm_util_append_expected_succeed = true;
bool is_strdup_expected_succeed = true;


extern "C" {
    int __wrap_orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data, opal_data_type_t type, char *units){
        if(!is_orcm_util_append_expected_succeed){
            return ORCM_ERROR;
        }
        return __real_orcm_util_append_orcm_value(list, key, data, type, units);
    }
    char* __wrap___strdup(const char *s)
    {
        if(!is_strdup_expected_succeed) {
            return NULL;
        }
        return __real___strdup(s);

    }
}

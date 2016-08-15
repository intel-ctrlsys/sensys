/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_PUGI_MOCKING_H
#define PARSER_PUGI_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    #include "opal/class/opal_list.h"
    #include "orcm/util/utils.h"

    extern int __real_orcm_util_append_orcm_value(opal_list_t *list, char *key,
                                                  void *data, opal_data_type_t type,
                                                  char *units);
    extern char* __real___strdup(const char *s);

#ifdef __cplusplus
}
#endif // __cplusplus


extern bool is_orcm_util_append_expected_succeed;
extern bool is_strdup_expected_succeed;

#endif // PARSER_PUGI_MOCKING_H

/*
 * Copyright (c) 2014-2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_UTIL_H
#define ORCM_UTIL_H

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/dss/dss_types.h"
#include "opal/class/opal_bitmap.h"

#include "orcm/mca/cfgi/cfgi_types.h"
#include "orcm/mca/analytics/analytics.h"

#define SAFEFREE(p) if(NULL!=p) {free(p); p=NULL;}
#define SAFE_RELEASE(p) if(NULL != p) OBJ_RELEASE(p);
#define MSG_HEADER ""
#define MSG_ERR_HEADER "\n"
#define MSG_FOOTER "\n"

#define ORCM_UTIL_MSG(txt) fprintf(stdout, MSG_HEADER txt MSG_FOOTER);
#define ORCM_UTIL_MSG_WITH_ARG(txt, arg) fprintf(stdout, MSG_HEADER txt MSG_FOOTER, arg);
#define ORCM_UTIL_ERROR_MSG(txt) fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER)
#define ORCM_UTIL_ERROR_MSG_WITH_ARG(txt, arg) \
            fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER, arg)

#define ORCM_UTIL_HASH_MULTIPLIER 31

ORCM_DECLSPEC void orcm_util_construct_uri(opal_buffer_t *buf,
                                           orcm_node_t *node);

ORCM_DECLSPEC int orcm_util_get_dependents(opal_list_t *targets,
                                           orte_process_name_t *root);
ORCM_DECLSPEC void orcm_util_print_xml(orcm_cfgi_xml_parser_t *x,
                                       char *pfx);

ORCM_DECLSPEC opal_value_t* orcm_util_load_opal_value(char *key, void *data,
                                                      opal_data_type_t type);

ORCM_DECLSPEC orcm_value_t* orcm_util_load_orcm_value(char *key, void *data,
                                               opal_data_type_t type, char *units);

/* create an orcm value and append it to a list,
 * the caller must make sure that the list is valid */
ORCM_DECLSPEC int orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data,
                                              opal_data_type_t type, char *units);

ORCM_DECLSPEC opal_value_t* orcm_util_copy_opal_value(opal_value_t* src);
ORCM_DECLSPEC orcm_value_t* orcm_util_copy_orcm_value(orcm_value_t* src);

/* copy a list of orcm_value_t to another existing list */
ORCM_DECLSPEC int orcm_util_copy_list_items(opal_list_t *src, opal_list_t *dest);

/* copy a list of orcm_value_t to another new list */
ORCM_DECLSPEC opal_list_t* orcm_util_copy_opal_list(opal_list_t *src);

/* get the number from the orcm_value_t and convert the number to double for computing */
ORCM_DECLSPEC double orcm_util_get_number_orcm_value(orcm_value_t *source_value);

ORCM_DECLSPEC orcm_analytics_value_t* orcm_util_load_orcm_analytics_value(opal_list_t *key,
                                                            opal_list_t *non_compute,
                                                            opal_list_t *compute);

/* create an orcm_analytics_value that retains the key and
 * non_compute data and assign the compute data to it */
ORCM_DECLSPEC orcm_analytics_value_t* orcm_util_load_orcm_analytics_value_compute(opal_list_t *key,
                                                            opal_list_t *non_compute,
                                                            opal_list_t *compute);

/* create an orcm_analytics_value that retains the key, update the time in non_compute data to the
 * current time, and assign the compute data to it */
ORCM_DECLSPEC orcm_analytics_value_t* orcm_util_load_analytics_time_compute(opal_list_t* key,
                                                                        opal_list_t* non_compute,
                                                                        opal_list_t* compute);


ORCM_DECLSPEC int orcm_util_find_items(const char *keys[], int num_keys, opal_list_t *list,
                             opal_value_t *items[], opal_bitmap_t *map);

ORCM_DECLSPEC uint64_t orcm_util_create_hash_key(void *key, size_t key_size);

/* calculate the time difference in seconds between time2 and time1. The result could be negative
 * if time2 is earlier than time1. The caller needs to check and take actions accordingly*/
ORCM_DECLSPEC double orcm_util_time_diff(struct timeval* time1, struct timeval* time2);

#endif

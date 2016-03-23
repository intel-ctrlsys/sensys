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
#define CHECK_NULL_ALLOC(x, e, label)  if(NULL==x) { e=ORCM_ERR_OUT_OF_RESOURCE; goto label; }

#define MSG_HEADER ""
#define MSG_ERR_HEADER "\n"
#define MSG_FOOTER "\n"

#define ORCM_UTIL_MSG(txt, ...) fprintf(stdout, MSG_HEADER txt MSG_FOOTER, ##__VA_ARGS__)
#define ORCM_UTIL_ERROR_MSG(txt) fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER)
#define ORCM_UTIL_ERROR_MSG_WITH_ARG(txt, arg) \
            fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER, arg)

/* In a few places, we need to barrier until something happens
 * that changes a flag to indicate we can release - e.g., waiting
 * for a specific message to arrive. If no progress thread is running,
 * we cycle across opal_progress - however, if a progress thread
 * is active, then we need to just nanosleep to avoid cross-thread
 * confusion. In addition, we set a timeout to avoid endless blocking.
 */
#define ORCM_WAIT_FOR_COMPLETION(flg, timeout, rc)                            \
    do {                                                                      \
        struct timeval start_time;                                            \
        struct timeval current_time;                                          \
        opal_output_verbose(1, orte_progress_thread_debug,                    \
                            "%s waiting on progress thread at %s:%d",         \
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),               \
                            __FILE__, __LINE__);                              \
        gettimeofday(&start_time, NULL);                                      \
        while ((flg)) {                                                       \
            gettimeofday(&current_time, NULL);                                \
            if (timeout <= orcm_util_time_diff(&start_time, &current_time)) { \
                break;                                                        \
            }                                                                 \
            /* provide a short quiet period so we                             \
             * don't hammer the cpu while waiting                             \
             */                                                               \
            struct timespec tp = {0, 100000};                                 \
            nanosleep(&tp, NULL);                                             \
        }                                                                     \
        *rc = ((flg) ? ORCM_ERROR : ORCM_SUCCESS);                            \
    }while(0);

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

/* create an orcm value and prepend it to a list,
 * the caller must make sure that the list is valid */
ORCM_DECLSPEC int orcm_util_prepend_orcm_value(opal_list_t *list, char *key, void *data,
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


/* Convert time from d/h/m/s format to seconds*/
ORCM_DECLSPEC unsigned long orcm_util_get_time_in_sec (char* time);

#endif

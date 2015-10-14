/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#define SAFEFREE(p) if(NULL!=p) {free(p); p=NULL;}
#define MSG_HEADER ""
#define MSG_ERR_HEADER "\n"
#define MSG_FOOTER "\n"

#define ORCM_UTIL_MSG(txt) fprintf(stdout, MSG_HEADER txt MSG_FOOTER);
#define ORCM_UTIL_MSG_WITH_ARG(txt, arg) fprintf(stdout, MSG_HEADER txt MSG_FOOTER, arg);
#define ORCM_UTIL_ERROR_MSG(txt) fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER)
#define ORCM_UTIL_ERROR_MSG_WITH_ARG(txt, arg) \
            fprintf(stderr, MSG_ERR_HEADER"ERROR: "txt MSG_FOOTER, arg)

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

ORCM_DECLSPEC opal_value_t* orcm_util_copy_opal_value(opal_value_t* src);
ORCM_DECLSPEC orcm_value_t* orcm_util_copy_orcm_value(orcm_value_t* src);

ORCM_DECLSPEC int find_items(const char *keys[], int num_keys, opal_list_t *list,
                             opal_value_t *items[], opal_bitmap_t *map);
#endif

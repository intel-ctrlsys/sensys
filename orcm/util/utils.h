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

#include "orcm/mca/cfgi/cfgi_types.h"

ORCM_DECLSPEC void orcm_util_construct_uri(opal_buffer_t *buf,
                                           orcm_node_t *node);

ORCM_DECLSPEC int orcm_util_get_dependents(opal_list_t *targets,
                                           orte_process_name_t *root);
ORCM_DECLSPEC void orcm_util_print_xml(orcm_cfgi_xml_parser_t *x,
                                       char *pfx);

ORCM_DECLSPEC opal_value_t* orcm_util_load_opal_value(char *key, void *data,
                                                      opal_data_type_t type);
#endif

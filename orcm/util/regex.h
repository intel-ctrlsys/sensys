/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_REGEX_H_
#define ORCM_REGEX_H_

#include "orcm_config.h"
#include "orcm/constants.h"


BEGIN_C_DECLS

/* NOTE: this is a destructive call for the nodes param - the
 * function will search and replace all commas with '\0'
 */
ORCM_DECLSPEC int orcm_regex_create(char *nodes, char **regexp);

ORCM_DECLSPEC int orcm_regex_extract_node_names(char *regexp, char ***names);

ORCM_DECLSPEC int orcm_regex_extract_names_list(char *regexp, char **namelist);

ORCM_DECLSPEC int orcm_regex_sort(char **nodes);

ORCM_DECLSPEC int orcm_regex_add_node(char **regexp, char *name);

ORCM_DECLSPEC int orcm_regex_remove_node(char **regexp, char *name);

END_C_DECLS
#endif

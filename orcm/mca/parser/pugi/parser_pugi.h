/*
 * Copyright (c) 2016 Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PARSER_PUGI_H
#define PARSER_PUGI_H

#include "orcm/mca/parser/parser.h"
#include "opal/class/opal_list.h"

BEGIN_C_DECLS

ORCM_DECLSPEC extern orcm_parser_base_component_t mca_parser_pugi_component;
ORCM_DECLSPEC extern orcm_parser_base_module_t orcm_parser_pugi_module;

extern int pugi_open(char const* file);
extern int pugi_close(int file_id);
extern opal_list_t* pugi_retrieve_document(int file_id);
extern opal_list_t* pugi_retrieve_section(int file_id, char const* key,
                                          char const* name);
extern opal_list_t* pugi_retrieve_section_from_list(int file_id,
                                          opal_list_item_t *start, char const* key,
                                          char const* name);
END_C_DECLS

#endif /* PARSER_PUGI_H */

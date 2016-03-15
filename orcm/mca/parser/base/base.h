/* Copyright (c) 2016      Intel, Inc.  All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_PARSER_BASE_H
#define MCA_PARSER_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */

#include "opal/class/opal_list.h"
#include "opal/mca/base/base.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/parser/parser.h"

BEGIN_C_DECLS

/*
 * MCA Framework
 */

typedef struct {
    opal_list_t actives;
} orcm_parser_base_t;

struct orcm_parser_active_module_t{
    opal_list_item_t super;
    int priority;
    mca_base_component_t *component;
    orcm_parser_base_module_t *module;
};
typedef struct orcm_parser_active_module_t orcm_parser_active_module_t;

OBJ_CLASS_DECLARATION(orcm_parser_active_module_t);

ORCM_DECLSPEC int orcm_parser_base_select(void);

ORCM_DECLSPEC extern mca_base_framework_t orcm_parser_base_framework;
ORCM_DECLSPEC extern orcm_parser_base_t orcm_parser_base;

ORCM_DECLSPEC int orcm_parser_base_open_file (char const *file);
ORCM_DECLSPEC int orcm_parser_base_close_file (int file_id);
ORCM_DECLSPEC opal_list_t* orcm_parser_base_retrieve_section (int file_id, opal_list_item_t *start,
                                                     char const* key, char const* name);
ORCM_DECLSPEC void orcm_parser_base_write_section (opal_list_t *result, int file_id, char const *key);

END_C_DECLS
#endif

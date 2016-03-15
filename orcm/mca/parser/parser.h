/* Copyright (c) 2016      Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * @file:
 *
 */

#ifndef MCA_PARSER_H
#define MCA_PARSER_H

/*
 * includes
 */

#include "orcm_config.h"
#include "orcm/types.h"

#include "orcm/mca/mca.h"

#include "orte/types.h"

BEGIN_C_DECLS


/* initialize the module */
typedef void (*orcm_parser_module_init_fn_t)(void);

/* finalize the module */
typedef void (*orcm_parser_module_fini_fn_t)(void);

typedef int (*orcm_parser_module_open_fn_t)(char const *file);

typedef int (*orcm_parser_module_close_fn_t)(int file_id);

typedef opal_list_t* (*orcm_parser_module_retrieve_section_fn_t)(int file_id,
                                opal_list_item_t *start, char const* key, char const* name);

typedef void (*orcm_parser_module_write_section_fn_t)(opal_list_t *result,
                                                      int file_id, char const *key);

/*
 * Ver 1.0
 */
struct orcm_parser_base_module_1_0_0_t {
    orcm_parser_module_init_fn_t              init;
    orcm_parser_module_fini_fn_t              finalize;
    orcm_parser_module_open_fn_t              open;
    orcm_parser_module_close_fn_t             close;
    orcm_parser_module_retrieve_section_fn_t  retrieve_section;
    orcm_parser_module_write_section_fn_t     write_section;
};

typedef struct orcm_parser_base_module_1_0_0_t orcm_parser_base_module_1_0_0_t;
typedef orcm_parser_base_module_1_0_0_t orcm_parser_base_module_t;


typedef int (*orcm_parser_base_API_open_file_fn_t) (char const *file);

typedef int (*orcm_parser_base_API_close_file_fn_t) (int file_id);

typedef opal_list_t* (*orcm_parser_base_API_retrieve_section_fn_t) (int file_id,
                                opal_list_item_t *start, char const* key, char const* name);

typedef void (*orcm_parser_base_API_write_section_fn_t) (opal_list_t *result,
                                                      int file_id, char const *key);

struct orcm_parser_API_module_1_0_0_t {
    orcm_parser_base_API_open_file_fn_t         open;
    orcm_parser_base_API_close_file_fn_t        close;
    orcm_parser_base_API_retrieve_section_fn_t  retrieve_section;
    orcm_parser_base_API_write_section_fn_t     write_section;
};

typedef struct orcm_parser_API_module_1_0_0_t orcm_parser_API_module_1_0_0_t;
typedef orcm_parser_API_module_1_0_0_t orcm_parser_API_module_t;

/*
 * the standard component data structure
 */
struct orcm_parser_base_component_1_0_0_t {
    mca_base_component_t base_version;
    mca_base_component_data_t base_data;
};
typedef struct orcm_parser_base_component_1_0_0_t orcm_parser_base_component_1_0_0_t;
typedef orcm_parser_base_component_1_0_0_t orcm_parser_base_component_t;

/*
 * Macro for use in components that are of type parser v1.0.0
 */
#define ORCM_parser_BASE_VERSION_1_0_0 \
  /* parser v1.0 is chained to MCA v2.0 */ \
    ORCM_MCA_BASE_VERSION_2_1_0("parser", 1, 0, 0)

/* Global structure for holding parser functions */
ORCM_DECLSPEC extern orcm_parser_API_module_t orcm_parser;

END_C_DECLS

#endif /* MCA_PARSER_H */

/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_DB_ZEROMQ_H
#define ORCM_DB_ZEROMQ_H

#include <stdio.h>

#include "opal/class/opal_pointer_array.h"

#include "orcm/mca/db/db.h"

BEGIN_C_DECLS

ORCM_MODULE_DECLSPEC extern orcm_db_base_component_t mca_db_zeromq_component;

typedef struct {
    orcm_db_base_module_t api;
    int bind_port;
} mca_db_zeromq_module_t;
ORCM_MODULE_DECLSPEC extern mca_db_zeromq_module_t mca_db_zeromq_module;

typedef void  (*orcm_db_zeromq_list_item_fn_t) (opal_list_item_t *item, char *tbuf);
typedef struct {
    const char *name;
    orcm_db_zeromq_list_item_fn_t zeromq;
} db_zeromq_types_t;

END_C_DECLS

#endif /* ORCM_DB_ZEROMQ_H */

/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_DB_PRINT_H
#define ORCM_DB_PRINT_H

#include <stdio.h>

#include "opal/class/opal_pointer_array.h"

#include "orcm/mca/db/db.h"

BEGIN_C_DECLS

ORCM_MODULE_DECLSPEC extern orcm_db_base_component_t mca_db_print_component;

typedef struct {
    orcm_db_base_module_t api;
    char *file;
    FILE *fp;
} mca_db_print_module_t;
ORCM_MODULE_DECLSPEC extern mca_db_print_module_t mca_db_print_module;

END_C_DECLS

#endif /* ORCM_DB_PRINT_H */

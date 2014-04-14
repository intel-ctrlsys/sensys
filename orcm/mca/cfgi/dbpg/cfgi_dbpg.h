/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_CFGI_DBPG_H
#define ORCM_CFGI_DBPG_H

#include "orcm/mca/cfgi/cfgi.h"

BEGIN_C_DECLS

typedef struct {
    orcm_cfgi_base_component_t super;
    char *dbname;
    char *table;
    char *user;
    char *pguri;
    char *pgoptions;
    char *pgtty;
} orcm_cfgi_dbpg_component_t;
ORCM_MODULE_DECLSPEC extern orcm_cfgi_dbpg_component_t mca_db_dbpg_component;

ORCM_DECLSPEC extern orcm_cfgi_base_module_t orcm_cfgi_dbpg_module;

END_C_DECLS

#endif /* ORCM_CFGI_POSTGRES_H */

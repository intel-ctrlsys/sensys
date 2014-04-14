/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_CFGI_MYSQL_H
#define ORCM_CFGI_MYSQL_H

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
} orcm_cfgi_mysql_component_t;
ORCM_MODULE_DECLSPEC extern orcm_cfgi_mysql_component_t mca_db_mysql_component;

ORCM_DECLSPEC extern orcm_cfgi_base_module_t orcm_cfgi_mysql_module;

END_C_DECLS

#endif /* ORCM_CFGI_MYSQL_H */

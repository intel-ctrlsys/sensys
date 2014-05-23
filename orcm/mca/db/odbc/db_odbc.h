/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_DB_ODBC_H
#define ORCM_DB_ODBC_H

#include <sqltypes.h>

#include "orcm/mca/db/db.h"

BEGIN_C_DECLS

ORCM_MODULE_DECLSPEC extern orcm_db_base_component_t mca_db_odbc_component;

typedef struct {
    orcm_db_base_module_t api;
    char *odbcdsn; /* ODBC Data Source Name */
    char *table;
    char *user;    
    SQLHENV envhandle;
    SQLHDBC dbhandle;
} mca_db_odbc_module_t;
ORCM_MODULE_DECLSPEC extern mca_db_odbc_module_t mca_db_odbc_module;

END_C_DECLS

#endif /* ORCM_DB_ODBC_H */

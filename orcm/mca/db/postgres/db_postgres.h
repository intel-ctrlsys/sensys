/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_DB_POSTGRES_H
#define ORCM_DB_POSTGRES_H

#include "libpq-fe.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/db/base/base.h"

char* build_query_from_function_name_and_arguments(const char* function_name, opal_list_t* arguments);
bool build_argument_string(char **argument_string, opal_list_t *argument_list);
char* format_opal_value_as_sql_string(opal_value_t *value);
char* format_string_to_sql(opal_data_type_t type, char* raw_string);
int check_for_invalid_characters(const char *function);

BEGIN_C_DECLS

ORCM_MODULE_DECLSPEC extern orcm_db_base_component_t mca_db_postgres_component;

typedef enum {
    ORCM_DB_PG_STMT_SET_NODE_FEATURE_STORE,
    ORCM_DB_PG_STMT_SET_NODE_FEATURE_UPDATE,
    ORCM_DB_PG_STMT_RECORD_DIAG_TEST_RESULT,
    ORCM_DB_PG_STMT_RECORD_DIAG_TEST_CONFIG,
    ORCM_DB_PG_STMT_ADD_EVENT,
    ORCM_DB_PG_STMT_ADD_EVENT_DATA,
    ORCM_DB_PG_STMT_NUM_STMTS
} orcm_db_postgres_prepared_statement_t;

typedef struct {
    orcm_db_base_module_t api;
    char *pguri;
    char *pgoptions;
    char *pgtty;
    char *dbname;
    char *user;
    PGconn *conn;
    bool autocommit;
    bool tran_started;
    bool prepared[ORCM_DB_PG_STMT_NUM_STMTS];
    opal_pointer_array_t *results_sets;
    int current_row;
} mca_db_postgres_module_t;
ORCM_MODULE_DECLSPEC extern mca_db_postgres_module_t mca_db_postgres_module;

END_C_DECLS

#endif /* ORCM_DB_POSTGRES_H */

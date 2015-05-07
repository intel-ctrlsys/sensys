/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_var.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/db/base/base.h"
#include "db_postgres.h"

static int component_register(void);
static bool component_avail(void);
static orcm_db_base_module_t *component_create(opal_list_t *props);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
orcm_db_base_component_t mca_db_postgres_component = {
    {
        ORCM_DB_BASE_VERSION_2_0_0,

        /* Component name and version */
        "postgres",
        ORCM_MAJOR_VERSION,
        ORCM_MINOR_VERSION,
        ORCM_RELEASE_VERSION,

        /* Component open and close functions */
        NULL,
        NULL,
        NULL,
        component_register
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    15,
    component_avail,
    component_create,
    NULL
};

static char *pguri;
static char *pgoptions;
static char *pgtty;
static char *dbname;
static char *user;
static bool autocommit;

static int component_register(void) {
    mca_base_component_t *c = &mca_db_postgres_component.base_version;
    
    /* retrieve the URI: <host>:<port> */
    pguri = NULL;
    (void)mca_base_component_var_register(c, "uri", "Postgres server URI: "
                                          "<host>:<port>",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                          0, OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &pguri);

    /* retrieve any options to be used */
    pgoptions = NULL;
    (void)mca_base_component_var_register(c, "options",
                                          "Additional Postgres options to pass "
                                          "to the database",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &pgoptions);
    
    /* retrieve the tty argument */
    pgtty = NULL;
    (void)mca_base_component_var_register(c, "tty", "TTY option for database",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                          0, OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &pgtty);
    

    /* retrieve the name of the database to be used */
    dbname = NULL;
    (void)mca_base_component_var_register(c, "database", "Name of database",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0,
                                          0, OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &dbname);

    /* retrieve the name of the user to be used */
    user = NULL;
    (void)mca_base_component_var_register(c, "user", "Database login: "
                                          "<user>:<password>",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &user);

    /* retrieve the auto commit setting */
    autocommit = true;
    (void)mca_base_component_var_register(c, "autocommit",
                                          "Auto commit",
                                          MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &autocommit);

    return ORCM_SUCCESS;
}

static bool component_avail(void)
{
    /* always available */
    return true;
}

static orcm_db_base_module_t *component_create(opal_list_t *props)
{
    mca_db_postgres_module_t *mod;
    opal_value_t *kv;

    mod = (mca_db_postgres_module_t*)malloc(sizeof(mca_db_postgres_module_t));
    if (NULL == mod) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return NULL;
    }
    memset(mod, 0, sizeof(mca_db_postgres_module_t));

    /* copy the APIs across */
    memcpy(mod, &mca_db_postgres_module.api, sizeof(orcm_db_base_module_t));

    /* assume default value first, then check for provided properties */
    mod->autocommit = autocommit;
    mod->tran_started = false;

    /* if the props include db info, then use it */
    if (NULL != props) {
        OPAL_LIST_FOREACH(kv, props, opal_value_t) {
            if (0 == strcmp(kv->key, "uri")) {
                mod->pguri = strdup(kv->data.string);
            } else if (0 == strcmp(kv->key, "options")) {
                mod->pgoptions = strdup(kv->data.string);
            } else if (0 == strcmp(kv->key, "tty")) {
                mod->pgtty = strdup(kv->data.string);
            } else if (0 == strcmp(kv->key, "database")) {
                mod->dbname = strdup(kv->data.string);
            } else if (0 == strcmp(kv->key, "user")) {
                mod->user = strdup(kv->data.string);
            } else if (0 == strcmp(kv->key, "autocommit")) {
                mod->autocommit = kv->data.flag;
            }
        }
    }

    if (NULL == mod->pguri) {
        if (NULL == pguri) {
            free(mod);
            return NULL;
        }
        mod->pguri = strdup(pguri);
    }

    if (NULL == mod->dbname) {
        if (NULL == dbname) {
            free(mod);
            return NULL;
        }
        mod->dbname = strdup(dbname);
    }

    /* all other entries are optional */
    if (NULL == mod->pgoptions && NULL != pgoptions) {
        mod->pgoptions = strdup(pgoptions);
    }
    if (NULL == mod->pgtty && NULL != pgtty) {
        mod->pgtty = strdup(pgtty);
    }
    if (NULL == mod->user && NULL != user) {
        mod->user = strdup(user);
    }

    /* let the module init */
    if (ORCM_SUCCESS != mod->api.init((struct orcm_db_base_module_t*)mod)) {
        mod->api.finalize((struct orcm_db_base_module_t*)mod);
        free(mod);
        return NULL;
    }

    return (orcm_db_base_module_t*)mod;
}

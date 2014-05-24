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
#include "db_odbc.h"

static int component_register(void);
static bool component_avail(void);
static orcm_db_base_module_t *component_create(opal_list_t *props);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
orcm_db_base_component_t mca_db_odbc_component = {
    {
        ORCM_DB_BASE_VERSION_2_0_0,

        /* Component name and version */
        "odbc",
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
    80,
    component_avail,
    component_create,
    NULL
};

static char *table;
static char *user;
static char *odbcdsn;

static int component_register(void) {
    mca_base_component_t *c = &mca_db_odbc_component.base_version;

    /* retrieve the data source name */
    odbcdsn = NULL;
    (void)mca_base_component_var_register(c, "odbcdsn",
                                          "ODBC data source name",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &odbcdsn);

    /* retrieve the name of the table to be used */
    table = NULL;
    (void)mca_base_component_var_register(c, "table", "Name of table",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &table);
     
    /* retrieve the name of the user to be used */
    user = NULL;
    (void)mca_base_component_var_register(c, "user",
                                          "Name of database user:password",
                                          MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                          OPAL_INFO_LVL_9,
                                          MCA_BASE_VAR_SCOPE_READONLY,
                                          &user);

    return OPAL_SUCCESS;
}

static bool component_avail(void)
{
    /* always available */
    return true;
}

static orcm_db_base_module_t *component_create(opal_list_t *props)
{
    mca_db_odbc_module_t *mod;
    opal_value_t *kv;

    mod = (mca_db_odbc_module_t*)malloc(sizeof(mca_db_odbc_module_t));
    if (NULL == mod) {
        ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
        return NULL;
    }
    memset(mod, 0, sizeof(mca_db_odbc_module_t));

    /* copy the APIs across */
    memcpy(mod, &mca_db_odbc_module.api, sizeof(orcm_db_base_module_t));

    /* if the props include db info, then use it */
    OPAL_LIST_FOREACH(kv, props, opal_value_t) {
        if (0 == strcmp(kv->key, "odbcdsn")) {
            mod->odbcdsn = strdup(kv->data.string);
        } else if (0 == strcmp(kv->key, "table")) {
            mod->table = strdup(kv->data.string);
        } else if (0 == strcmp(kv->key, "user")) {
            mod->user = strdup(kv->data.string);
        }
    }
    if (NULL == mod->odbcdsn) {
        if (NULL == odbcdsn) {
            /* nothing was provided - opt out */
            free(mod);
            return NULL;
        }
    }
    if (NULL == mod->table) {
        if (NULL == table) {
            /* nothing was provided - opt out */
            free(mod);
            return NULL;
        }
        mod->table = strdup(table);
    }
    /* all other entries are optional */
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

/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
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

#include "orcm/mca/cfgi/cfgi.h"
#include "cfgi_dbmysql.h"

static int postgres_component_open(void);
static int postgres_component_close(void);
static int postgres_component_query(mca_base_module_t **module, int *priority);
static int postgres_component_register(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
orcm_cfgi_mysql_component_t mca_cfgi_mysql_component = {
    {
        {
            ORCM_CFGI_BASE_VERSION_1_0_0,

            /* Component name and version */
            "mysql",
            ORCM_MAJOR_VERSION,
            ORCM_MINOR_VERSION,
            ORCM_RELEASE_VERSION,

            /* Component open and close functions */
            postgres_component_open,
            postgres_component_close,
            postgres_component_query,
            postgres_component_register
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
    }
};


static int postgres_component_open(void)
{
    return ORCM_SUCCESS;
}


static int postgres_component_query(mca_base_module_t **module, int *priority)
{

    if (NULL != mca_cfgi_mysql_component.dbname &&
        NULL != mca_cfgi_mysql_component.table &&
        NULL != mca_cfgi_mysql_component.user &&
        NULL != mca_cfgi_mysql_component.pguri) {
        *priority = 90;
        *module = (mca_base_module_t*)&orcm_cfgi_mysql_module;
        return ORCM_SUCCESS;
    }

    *priority = 0;
    *module = NULL;
    return ORCM_ERROR;
}


static int postgres_component_close(void)
{
    return ORCM_SUCCESS;
}

static int postgres_component_register(void) {
    mca_base_component_t *c = &mca_cfgi_mysql_component.super.base_version;

    /* retrieve the name of the database to be used */
    mca_cfgi_mysql_component.dbname = NULL;
    (void) mca_base_component_var_register (c, "database", "Name of database",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.dbname);

    /* retrieve the name of the table to be used */
    mca_cfgi_mysql_component.table = NULL;
    (void) mca_base_component_var_register (c, "table", "Name of table",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.table);
     
    /* retrieve the name of the user to be used */
    mca_cfgi_mysql_component.user = NULL;
    (void) mca_base_component_var_register (c, "user", "Name of database user:password",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.user);
    
    /* retrieve the server:port */
    mca_cfgi_mysql_component.pguri = NULL;
    (void) mca_base_component_var_register (c, "uri", "Contact info for Postgres server as ip:port",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.pguri);
    
    /* retrieve any options to be used */
    mca_cfgi_mysql_component.pgoptions = NULL;
    (void) mca_base_component_var_register (c, "options", "Options to pass to the database",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.pgoptions);
    
    /* retrieve the tty argument */
    mca_cfgi_mysql_component.pgtty = NULL;
    (void) mca_base_component_var_register (c, "tty", "TTY option for database",
                                            MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_cfgi_mysql_component.pgtty);
    return ORCM_SUCCESS;
}

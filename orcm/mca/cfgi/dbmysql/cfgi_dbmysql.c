/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/cfgi/dbmysql/cfgi_dbmysql.h"

/* API functions */

static int mysql_init(void);
static void mysql_finalize(void);
static int read_config(opal_list_t *config);
static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf);

/* The module struct */

orcm_cfgi_base_module_t orcm_cfgi_mysql_module = {
    mysql_init,
    mysql_finalize,
    read_config,
    define_system
};

static int mysql_init(void)
{
    return ORCM_ERR_TAKE_NEXT_OPTION;
}

static void mysql_finalize(void)
{
}

static int read_config(opal_list_t *config)
{
    return ORCM_ERR_TAKE_NEXT_OPTION;
}

static int define_system(opal_list_t *config,
                         orcm_node_t **mynode,
                         orte_vpid_t *num_procs,
                         opal_buffer_t *buf)
{
    return ORCM_ERR_TAKE_NEXT_OPTION;
}

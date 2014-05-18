/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/mca/installdirs/installdirs.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/cfgi/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/cfgi/base/static-components.h"

/*
 * Global variables
 */
orcm_cfgi_base_t orcm_cfgi_base;
orcm_cfgi_API_module_t orcm_cfgi = {
    orcm_cfgi_base_read_config,
    orcm_cfgi_base_define_sys
};

static int orcm_cfgi_base_register(mca_base_register_flag_t flags)
{
    /* check for file name */
    asprintf(&orcm_cfgi_base.config_file, "%s/etc/orcm-site.xml", opal_install_dirs.prefix);
    (void) mca_base_var_register("orcm", "cfgi", "base", "config_file",
                                 "ORCM configuration file",
                                 MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_cfgi_base.config_file);

    /* see if we want to periodically check for changes in the config
     * file - if we see them, then we automatically update our config.
     * Note that this will be disabled if the OS supports file change
     * notifications
     */
    orcm_cfgi_base.ping_rate = -1;
    (void) mca_base_var_register("orcm", "cfgi", "base", "ping_rate",
                                 "How often to check the ORCM config file for changes, in seconds [default: disabled]",
                                 MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_cfgi_base.ping_rate);

    /* do we want to just validate the config? */
    orcm_cfgi_base.validate = false;
    (void) mca_base_var_register("orcm", "cfgi", "base", "validate",
                                 "Validate the ORCM configuration file",
                                 MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &orcm_cfgi_base.validate);

    return ORCM_SUCCESS;
}

static int orcm_cfgi_base_close(void)
{
    orcm_cfgi_base_active_t *active;

    /* Close the active modules */
    OPAL_LIST_FOREACH(active, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
        if( NULL != active->mod->finalize ) {
            active->mod->finalize();
        }
    }
    OPAL_LIST_DESTRUCT(&orcm_cfgi_base.actives);

    return mca_base_framework_components_close(&orcm_cfgi_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_cfgi_base_open(mca_base_open_flag_t flags)
{
    opal_data_type_t tmp;
    int rc;

    /* construct globals */
    OBJ_CONSTRUCT(&orcm_cfgi_base.actives, opal_list_t);
    orcm_cfgi_base.ping_rate = 0;
    orcm_cfgi_base.validate = false;

    /* register the configure types for packing/unpacking services */
    tmp = ORCM_NODE;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_node,
                                                     orcm_unpack_node,
                                                     (opal_dss_copy_fn_t)orcm_copy_node,
                                                     (opal_dss_compare_fn_t)orcm_compare_node,
                                                     (opal_dss_print_fn_t)orcm_print_node,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_NODE", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }
    tmp = ORCM_RACK;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_rack,
                                                     orcm_unpack_rack,
                                                     (opal_dss_copy_fn_t)orcm_copy_rack,
                                                     (opal_dss_compare_fn_t)orcm_compare_rack,
                                                     (opal_dss_print_fn_t)orcm_print_rack,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_RACK", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }
    tmp = ORCM_ROW;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_row,
                                                     orcm_unpack_row,
                                                     (opal_dss_copy_fn_t)orcm_copy_row,
                                                     (opal_dss_compare_fn_t)orcm_compare_row,
                                                     (opal_dss_print_fn_t)orcm_print_row,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_ROW", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }
    tmp = ORCM_CLUSTER;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_cluster,
                                                     orcm_unpack_cluster,
                                                     (opal_dss_copy_fn_t)orcm_copy_cluster,
                                                     (opal_dss_compare_fn_t)orcm_compare_cluster,
                                                     (opal_dss_print_fn_t)orcm_print_cluster,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_CLUSTER", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }
    tmp = ORCM_SCHEDULER;
    if (OPAL_SUCCESS != (rc = opal_dss.register_type(orcm_pack_scheduler,
                                                     orcm_unpack_scheduler,
                                                     (opal_dss_copy_fn_t)orcm_copy_scheduler,
                                                     (opal_dss_compare_fn_t)orcm_compare_scheduler,
                                                     (opal_dss_print_fn_t)orcm_print_scheduler,
                                                     OPAL_DSS_STRUCTURED,
                                                     "ORCM_SCHEDULER", &tmp))) {
        ORTE_ERROR_LOG(rc);
    }

    return mca_base_framework_components_open(&orcm_cfgi_base_framework, flags);
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, cfgi, "ORCM Configuration Framework",
                           orcm_cfgi_base_register,
                           orcm_cfgi_base_open, orcm_cfgi_base_close,
                           mca_cfgi_base_static_components, 0);


OBJ_CLASS_INSTANCE(orcm_cfgi_base_active_t,
                   opal_list_item_t,
                   NULL, NULL);

static void xml_cons(orcm_cfgi_xml_parser_t *ptr)
{
    ptr->name = NULL;
    ptr->value = NULL;
    OBJ_CONSTRUCT(&ptr->subvals, opal_list_t);
}
static void xml_dest(orcm_cfgi_xml_parser_t *ptr)
{
    if (NULL != ptr->name) {
        free(ptr->name);
    }
    if (NULL != ptr->value) {
        opal_argv_free(ptr->value);
    }
    OPAL_LIST_DESTRUCT(&ptr->subvals);
}
OBJ_CLASS_INSTANCE(orcm_cfgi_xml_parser_t,
                   opal_list_item_t,
                   xml_cons, xml_dest);

static void cfg_cons(orcm_config_t* p)
{
    p->mca_params = NULL;
    p->env = NULL;
    p->port = NULL;
    p->aggregator = false;
}
static void cfg_des(orcm_config_t *p)
{
    if (NULL != p->mca_params) {
        opal_argv_free(p->mca_params);
    }
    if (NULL != p->env) {
        opal_argv_free(p->env);
    }
    if (NULL != p->port) {
        free(p->port);
    }
}
OBJ_CLASS_INSTANCE(orcm_config_t,
                   opal_object_t,
                   cfg_cons, cfg_des);

static void nd_con(orcm_node_t *p)
{
    p->name = NULL;
    p->rack = NULL;
    p->daemon = *ORTE_NAME_INVALID;
    OBJ_CONSTRUCT(&p->config, orcm_config_t);
    p->state = ORCM_NODE_STATE_UNDEF;
    p->scd_state = ORCM_SCD_NODE_STATE_UNDEF;
}
static void nd_des(orcm_node_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    if (NULL != p->rack) {
        OBJ_RELEASE(p->rack);
    }
    OBJ_DESTRUCT(&p->config);
}
OBJ_CLASS_INSTANCE(orcm_node_t,
                   opal_list_item_t,
                   nd_con, nd_des);

static void rk_con(orcm_rack_t *p)
{
    p->name = NULL;
    p->row = NULL;
    OBJ_CONSTRUCT(&p->controller, orcm_node_t);
    OBJ_CONSTRUCT(&p->nodes, opal_list_t);
}
static void rk_des(orcm_rack_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    if (NULL != p->row) {
        OBJ_RELEASE(p->row);
    }
    OBJ_DESTRUCT(&p->controller);
    OPAL_LIST_DESTRUCT(&p->nodes);
}
OBJ_CLASS_INSTANCE(orcm_rack_t,
                   opal_list_item_t,
                   rk_con, rk_des);

static void row_con(orcm_row_t *p)
{
    p->name = NULL;
    p->cluster = NULL;
    OBJ_CONSTRUCT(&p->controller, orcm_node_t);
    OBJ_CONSTRUCT(&p->racks, opal_list_t);
}
static void row_des(orcm_row_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    if (NULL != p->cluster) {
        OBJ_RELEASE(p->cluster);
    }
    OBJ_DESTRUCT(&p->controller);
    OPAL_LIST_DESTRUCT(&p->racks);
}
OBJ_CLASS_INSTANCE(orcm_row_t,
                   opal_list_item_t,
                   row_con, row_des);

static void cl_con(orcm_cluster_t *p)
{
    p->name = NULL;
    OBJ_CONSTRUCT(&p->controller, orcm_node_t);
    OBJ_CONSTRUCT(&p->rows, opal_list_t);
}
static void cl_des(orcm_cluster_t *p)
{
    if (NULL != p->name) {
        free(p->name);
    }
    OBJ_DESTRUCT(&p->controller);
    OPAL_LIST_DESTRUCT(&p->rows);
}
OBJ_CLASS_INSTANCE(orcm_cluster_t,
                   opal_list_item_t,
                   cl_con, cl_des);

static void sc_con(orcm_scheduler_t *p)
{
    OBJ_CONSTRUCT(&p->controller, orcm_node_t);
    p->queues = NULL;
}
static void sc_des(orcm_scheduler_t *p)
{
    OBJ_DESTRUCT(&p->controller);
    if (NULL != p->queues) {
        opal_argv_free(p->queues);
    }
}
OBJ_CLASS_INSTANCE(orcm_scheduler_t,
                   opal_list_item_t,
                   sc_con, sc_des);

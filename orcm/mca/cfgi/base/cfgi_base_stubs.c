/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/types.h"
#include "orcm/constants.h"

#include <sys/types.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/cfgi/base/base.h"


int orcm_cfgi_base_read_config(opal_list_t *config)
{
    orcm_cfgi_base_active_t *a;
    bool rd;
    int rc;

    rd = false;
    OPAL_LIST_FOREACH(a, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
        if (NULL == a->mod->read_config) {
            continue;
        }
        if (ORCM_SUCCESS == (rc = a->mod->read_config(config))) {
            rd = true;
            break;
        }
        if (ORCM_ERR_TAKE_NEXT_OPTION != rc) {
            /* plugins return "next option" if they can't read
             * the config. anything else is a true error.
             */
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    if (!rd) {
        /* nobody could read the config */
        ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

int orcm_cfgi_base_define_sys(opal_list_t *config,
                              orcm_node_t **mynode,
                              orte_vpid_t *num_procs,
                              opal_buffer_t *buf)
{
    orcm_cfgi_base_active_t *a;
    bool rd;
    int rc;

    rd = false;
    OPAL_LIST_FOREACH(a, &orcm_cfgi_base.actives, orcm_cfgi_base_active_t) {
        if (NULL == a->mod->define_system) {
            continue;
        }
        if (ORCM_SUCCESS == (rc = a->mod->define_system(config, mynode, num_procs, buf))) {
            rd = true;
            break;
        }
        if (ORCM_ERR_TAKE_NEXT_OPTION != rc) {
            /* plugins return "next option" if they can't read
             * the define system. anything else is a true error.
             */
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    if (!rd) {
        /* nobody could parse the config */
        ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
        return ORCM_ERR_BAD_PARAM;
    }
    return ORCM_SUCCESS;
}

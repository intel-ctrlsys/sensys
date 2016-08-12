/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/mca/base/base.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/db/db.h"
#include "orcm/mca/db/base/base.h"
#include "orcm/util/utils.h"
#include "db_zeromq.h"

int orcm_db_zeromq_component_register(void);
bool orcm_db_zeromq_component_avail(void);
orcm_db_base_module_t* orcm_db_zeromq_component_create(opal_list_t *props);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
orcm_db_base_component_t mca_db_zeromq_component = {
    {
        ORCM_DB_BASE_VERSION_2_0_0,

         /* Component name and version */
        .mca_component_name = "zeromq",
        MCA_BASE_MAKE_VERSION(component, ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,
                              ORCM_RELEASE_VERSION),

        /* Component open and close functions */
        .mca_open_component = NULL,
        .mca_close_component = NULL,
        .mca_query_component = NULL,
        .mca_register_component_params = orcm_db_zeromq_component_register
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    .priority = 7,
    .available = orcm_db_zeromq_component_avail,
    .create_handle = orcm_db_zeromq_component_create,
    .finalize = NULL
};

int orcm_db_zeromq_bind_port = 37001; /* Default bind port */

int orcm_db_zeromq_component_register(void)
{
    (void) mca_base_component_var_register (&mca_db_zeromq_component.base_version,
                                            "bind_port", "Send JSON data to the indicated ZeroMQ port",
                                            MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &orcm_db_zeromq_bind_port);

    return ORCM_SUCCESS;
}

bool orcm_db_zeromq_component_avail(void)
{
    /* always available */
    return true;
}

#define ORCM_ON_NULL_REPORT_AND_RETURN_NULL(x) \
    if(NULL == (x)){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return NULL;}
orcm_db_base_module_t* orcm_db_zeromq_component_create(opal_list_t *props)
{
    mca_db_zeromq_module_t *mod;

    mod = (mca_db_zeromq_module_t*)malloc(sizeof(mca_db_zeromq_module_t));
    ORCM_ON_NULL_REPORT_AND_RETURN_NULL(mod);

    /* copy the APIs across */
    memcpy(mod, &mca_db_zeromq_module.api, sizeof(orcm_db_base_module_t));

    /* set the globals */
    mod->bind_port = orcm_db_zeromq_bind_port;

    /* let the module init */
    if (ORCM_SUCCESS != mod->api.init((struct orcm_db_base_module_t*)mod)) {
        ORTE_ERROR_LOG(ORCM_ERR_NOT_INITIALIZED);
        mod->api.finalize((struct orcm_db_base_module_t*)mod);
        SAFEFREE(mod);
        return NULL;
    }

    return (orcm_db_base_module_t*)mod;
}

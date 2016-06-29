/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/include/orcm/constants.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/class/opal_list.h"
#include "orte/types.h"
#include "orcm/mca/cfgi/cfgi_types.h"

int mock_cfgi_read_config(opal_list_t *config);
int mock_cfgi_define_system(opal_list_t *config,
                            orcm_node_t **mynode,
                            orte_vpid_t *num_procs,
                            opal_buffer_t *buf);
extern "C" {
    int __real_opal_pointer_array_init(opal_pointer_array_t *array,
                                       int initial_allocation,
                                       int max_size,
                                       int block_size);
    int __real_mca_base_framework_open(struct mca_base_framework_t *framework,
                                       mca_base_open_flag_t flags);
    int __real_opal_dss_unpack(opal_buffer_t *buffer,
                         void *dst,
                         int32_t *num_vals,
                         opal_data_type_t type);
}

typedef struct mocking_flags {
    bool orte_ess_base_std_prolog;
    bool opal_pointer_array_init;
    int opal_pointer_array_init_max;
    bool mca_base_framework_open;
    int mca_base_framework_open_max;
    int framework_open_counter;
    bool read_config;
    bool define_system;
    bool orte_plm_base_set_hnp_name;
    bool orte_register_params;
    bool opal_dss_unpack;
    bool opal_pstat_base_select;
    bool orte_state_base_select;
    bool orte_oob_base_select;
    bool orte_rml_base_select;
    bool orte_errmgr_base_select;
    bool orte_routed_base_select;
    bool orte_rmaps_base_select;
    bool orcm_db_base_select;
    bool opal_dstore_base_select;
}mocking_flags;

extern mocking_flags sst_mocking;

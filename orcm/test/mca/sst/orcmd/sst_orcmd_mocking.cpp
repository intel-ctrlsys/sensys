/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sst_orcmd_mocking.h"
#include <iostream>

mocking_flags sst_mocking;

extern "C" {
    int __wrap_orte_ess_base_std_prolog(void) {
        if (sst_mocking.orte_ess_base_std_prolog) {
            return ORCM_ERROR;
        } else {
            return ORCM_SUCCESS;
        }
    }

    // This function is called several times inside the tested function
    // and because of this is necessary to implement a wrapper that will
    // execute the real function until the opal_pointer_array_init_max is
    // reach.
    int __wrap_opal_pointer_array_init(opal_pointer_array_t *array,
                                       int initial_allocation,
                                       int max_size,
                                       int block_size)
    {
        static int calls_count = 0;
        calls_count++;
        if (calls_count < sst_mocking.opal_pointer_array_init_max ||
            !sst_mocking.opal_pointer_array_init ) {
            return __real_opal_pointer_array_init(array,
                                                  initial_allocation,
                                                  max_size,
                                                  block_size);
        } else if (sst_mocking.opal_pointer_array_init) {
            if (calls_count == sst_mocking.opal_pointer_array_init_max) {
                calls_count = 0;
            }
            return ORCM_ERROR;
        }
    }

    int __wrap_mca_base_framework_open(struct mca_base_framework_t *framework,
                             mca_base_open_flag_t flags)
    {
        if(sst_mocking.mca_base_framework_open){
            if (sst_mocking.framework_open_counter++ >=
                sst_mocking.mca_base_framework_open_max){
                return ORCM_ERROR;
            }
        }
        return __real_mca_base_framework_open(framework, flags);
    }

    int __wrap_orte_register_params(void) {
        if (sst_mocking.orte_register_params) {
            return ORCM_ERROR;
        } else {
            return ORCM_SUCCESS;
        }
    }

    int __wrap_opal_dss_unpack(opal_buffer_t *buffer,
                               void *dst,
                               int32_t *num_vals,
                               opal_data_type_t type)
    {
        void **tmpresult = (void**) dst;
        if(sst_mocking.opal_dss_unpack){
            if(type == OPAL_BUFFER){
                *tmpresult = OBJ_NEW(opal_buffer_t);
                return ORCM_SUCCESS;
            }
        }
        return __real_opal_dss_unpack(buffer, dst, num_vals, type);
    }
}

int mock_cfgi_read_config(opal_list_t *config)
{
    if (sst_mocking.read_config) {
        return ORCM_ERROR;
    } else {
        return ORCM_SUCCESS;
    }
}

int mock_cfgi_define_system(opal_list_t *config,
                            orcm_node_t **mynode,
                            orte_vpid_t *num_procs,
                            opal_buffer_t *buf) {

    orcm_rack_t *rack = (orcm_rack_t*)malloc(sizeof(orcm_rack_t));
    orcm_node_t *controller = (orcm_node_t*)malloc(sizeof(orcm_node_t));
    *mynode = &rack->controller;

    if (sst_mocking.define_system) {
        return ORCM_ERROR;
    } else {
        return ORCM_SUCCESS;
    }
}


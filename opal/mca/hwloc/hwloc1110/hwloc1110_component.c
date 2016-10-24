/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC. All rights
 *                         reserved.
 *
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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

#include "opal_config.h"
#include "opal/constants.h"

#include "opal/mca/hwloc/hwloc.h"
#include "hwloc1110.h"

/*
 * Public string showing the sysinfo ompi_linux component version number
 */
const char *opal_hwloc_hwloc1110_component_version_string =
    "OPAL hwloc1110 hwloc MCA component version " OPAL_VERSION;

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

const opal_hwloc_component_t mca_hwloc_hwloc1110_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    .base_version = {
        OPAL_HWLOC_BASE_VERSION_2_0_0,

        /* Component name and version */
        .mca_component_name = "hwloc1110",
        MCA_BASE_MAKE_VERSION(component,  OPAL_MAJOR_VERSION, OPAL_MINOR_VERSION,
                              OPAL_RELEASE_VERSION),
    },
    .base_data = {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};

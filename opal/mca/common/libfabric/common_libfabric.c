/*
 * Copyright (c) 2015      Intel Corporation.  All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "opal_config.h"
#include "opal/constants.h"

#include <errno.h>
#include <unistd.h>

#include "common_libfabric.h"

int mca_common_libfabric_register_mca_variables(void)
{
    return OPAL_SUCCESS;
}

/*
 * Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>

int orcm_err2str(int errnum, const char **errmsg)
{
    switch (errnum) {
        case ORCM_ERR_PLACEHOLDER:
        *errmsg = "Placeholder";
        break;
        case ORCM_ERR_SENSOR_READ_FAIL:
        *errmsg = "Unable to read sensor data";
        break;
        default:
            *errmsg = NULL;
    }

    return ORCM_SUCCESS;
}

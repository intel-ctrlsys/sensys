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
        case ORCM_ERR_NO_ANY_GROUP:
        *errmsg = "There are no groups defined yet!";
        break;
        case ORCM_ERR_GROUP_NOT_EXIST:
        *errmsg = "The requested group does not exist!";
        break;
        case ORCM_ERR_NODE_NOT_EXIST:
        *errmsg = "Some node(s) does(do) not exist in the requested group(s)!";
        break;
        case ORCM_ERR_IPMI_CONFLICT:
        *errmsg = "Unable to perform IPMI operation. IPMI operations on different threads are not supported.";
        break;
        case ORCM_ERR_BMC_INFO_NOT_FOUND:
        *errmsg = "No BMC information was found for that node!";
        break;
        default:
            *errmsg = NULL;
    }

    return ORCM_SUCCESS;
}

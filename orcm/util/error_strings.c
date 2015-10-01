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
        case ORCM_ERR_ALL_NODE_EXIST:
        *errmsg = "All the nodes already exist!";
        break;
        case ORCM_ERR_PART_NODE_EXIST:
        *errmsg = "A part of the nodes already exist!";
        break;
        case ORCM_ERR_GROUP_NOT_EXIST:
        *errmsg = "There is no such group!";
        break;
        case ORCM_ERR_NONE_NODE_EXIST:
        *errmsg = "None of the nodes exist!";
        break;
        case ORCM_ERR_PART_NODE_NOT_EXIST:
        *errmsg = "A part of the nodes do not exist!";
        break;
        case ORCM_ERR_NO_RECORD:
        *errmsg = "There is no record!";
        break;
        case ORCM_ERR_INCORRECT_ARGUMENTS:
        *errmsg = "incorrect arguments!";
        break;
        default:
            *errmsg = NULL;
    }

    return ORCM_SUCCESS;
}

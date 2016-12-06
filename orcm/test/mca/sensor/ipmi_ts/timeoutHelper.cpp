/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "timeoutHelper.hpp"

double elapsedSecs(time_t startTime)
{
    time_t now;
    time(&now);
    return difftime(now, startTime);
}

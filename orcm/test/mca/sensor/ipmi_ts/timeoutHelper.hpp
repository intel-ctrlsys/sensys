/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef TIMEOUT_HELPER_H
#define TIMEOUT_HELPER_H

#include <ctime>

#define TIMEOUT 5

#define assertTimedOut(f, TO, INV) \
    {                                                           \
        bool timedOut = false;                                  \
        time_t begin;                                           \
        time(&begin);                                           \
        while (f!=INV && !(timedOut = elapsedSecs(begin) > TO)) \
            usleep(100);                                        \
        ASSERT_FALSE(timedOut);                                 \
    }

#define ASSERT_FALSE_TIMEOUT(val, timeout) \
    {                                      \
        assertTimedOut(val, timeout, false);  \
    }

#define ASSERT_TRUE_TIMEOUT(val, timeout)   \
    {                                       \
        assertTimedOut(val, timeout, true); \
    }

extern double elapsedSecs(time_t startTime);

#endif

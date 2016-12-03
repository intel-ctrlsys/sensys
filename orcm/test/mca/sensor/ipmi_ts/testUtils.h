/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef GREI_TESTUTILS_H
#define GREI_TESTUTILS_H

#include <queue>

class mockManager
{
private:
    std::queue<int> pendingMocks;
public:
    inline void restartMock()
    {
        pendingMocks = std::queue<int>();
    }

    inline int nextMockState()
    {
        if (pendingMocks.empty())
            return 0;

        int retValue = pendingMocks.front();
        pendingMocks.pop();

        return retValue;
    }

    inline void pushState(int s)
    {
        pendingMocks.push(s);
    }
};

#define assertTimedOut(f, TO, INV) \
    {                                                           \
        bool timedOut = false;                                  \
        time_t begin;                                           \
        time(&begin);                                           \
        while (f!=INV && !(timedOut = elapsedSecs(begin) > TO)) \
            usleep(100);                                        \
        ASSERT_FALSE(timedOut);                                 \
    }


#endif //GREI_TESTUTILS_H

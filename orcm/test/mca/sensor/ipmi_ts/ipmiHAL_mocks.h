/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIHAL_MOCKS_H
#define IPMIHAL_MOCKS_H

#include "testUtils.h"
extern "C"
{
    #include "opal/mca/event/libevent2022/libevent2022.h"
};

enum supportedMocks
{
    OPAL_EVENT_EVTIMER_NEW
};

static std::map<supportedMocks, mockManager> mocks;

enum mockStates
{
    NO_MOCK = 0,
    SUCCESS,
    FAILURE,
};

extern "C"
{
    extern struct event *__real_opal_libevent2022_event_new(struct event_base *,
                                                            evutil_socket_t,
                                                            short,
                                                            event_callback_fn,
                                                            void *);
    struct event *__wrap_opal_libevent2022_event_new(struct event_base *a,
                                                     evutil_socket_t b,
                                                     short c,
                                                     event_callback_fn d,
                                                     void *e)
    {
        mockStates state = (mockStates) mocks[OPAL_EVENT_EVTIMER_NEW].nextMockState();
        if (NO_MOCK == state)
            return __real_opal_libevent2022_event_new(a, b, c, d, e);

        switch (state)
        {
            default:
                return NULL;
        }
    }
};
#endif // IPMIHAL_MOCKS_H

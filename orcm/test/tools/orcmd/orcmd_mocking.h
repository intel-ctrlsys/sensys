/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCMD_MOCKING_H
#define ORCMD_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    extern int __real_orcm_init(int flags);

#ifdef __cplusplus
}
#endif // __cplusplus


typedef void (*orcm_init_callback_fn_t)(void);


class orcmd_tests_mocking
{
    public:
        // Construction
        orcmd_tests_mocking();

        // Public Callbacks
        orcm_init_callback_fn_t orcm_init_callback;
};

extern orcmd_tests_mocking orcmd_mocking;

#endif // ORCMD_MOCKING_H

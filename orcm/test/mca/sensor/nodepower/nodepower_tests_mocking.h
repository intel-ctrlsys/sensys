/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef NODEPOWER_TESTS_MOCKING_H
#define NODEPOWER_TESTS_MOCKING_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include "orte/mca/state/state.h"

    extern int __real_ipmi_cmdraw(unsigned char,unsigned char,unsigned char,
                                  unsigned char,unsigned char,unsigned char*,
                                  int,unsigned char*,int*,unsigned char*,unsigned char);
    extern int __real_ipmi_close(void);
    extern __uid_t __real_geteuid(void);
    extern struct tm* __real_localtime(const time_t*);
#ifdef __cplusplus
}
#endif // __cplusplus

typedef int (*ipmi_cmdraw_fn_t)(unsigned char,unsigned char,unsigned char,
                                unsigned char,unsigned char,unsigned char*,
                                int,unsigned char*,int*,unsigned char*,unsigned char);
typedef int (*ipmi_close_fn_t)(void);
typedef __uid_t (*geteuid_fn_t)(void);
typedef struct tm* (*localtime_fn_t)(const time_t*);

class nodepower_tests_mocking
{
    public:
        // Construction
        nodepower_tests_mocking();

        // Public Callbacks
        ipmi_cmdraw_fn_t ipmi_cmdraw_callback;
        ipmi_close_fn_t ipmi_close_callback;
        geteuid_fn_t geteuid_callback;
        localtime_fn_t localtime_callback;
};

extern nodepower_tests_mocking nodepower_mocking;

#endif // NODEPOWER_TESTS_MOCKING_H

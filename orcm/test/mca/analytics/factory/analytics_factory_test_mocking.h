/*
 * Copyright (c) 2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_MOCKING_H_
#define ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_MOCKING_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
    #include <dirent.h>
    extern struct dirent* __real_readdir(DIR*);
    extern void* __real_dlopen(const char*, int);
    extern char* __real_dlerror(void);
    extern void* __real_dlsym(void *__restrict, const char *__restrict);
    extern int __real_dlclose(void*);
#ifdef __cplusplus
}
#endif // __cplusplus

typedef struct dirent* (*readdir_callback_fn_t)(DIR*);
typedef void* (*dlopen_callback_fn_t)(const char*, int);
typedef char* (*dlerror_callback_fn_t)(void);
typedef void* (*dlsym_callback_fn_t)(void*, const char*);
typedef int (*dlclose_callback_fn_t)(void*);

class AnalyticsFactoryTestMocking
{
    public:
        // Public Callbacks
        readdir_callback_fn_t readdir_callback;
        dlopen_callback_fn_t dlopen_callback;
        dlerror_callback_fn_t dlerror_callback;
        dlsym_callback_fn_t dlsym_callback;
        dlclose_callback_fn_t dlclose_callback;
};

extern AnalyticsFactoryTestMocking analytics_factory_mocking;



#endif /* ORCM_TEST_MCA_ANALYTICS_FACTORY_ANALYTICS_FACTORY_TEST_MOCKING_H_ */

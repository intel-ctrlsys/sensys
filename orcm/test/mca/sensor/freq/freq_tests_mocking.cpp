/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "freq_tests_mocking.h"

#include <iostream>
using namespace std;

freq_tests_mocking freq_mocking;


extern "C" { // Mocking must use correct "C" linkages
    DIR* __wrap_opendir(const char* dirname)
    {
        if(NULL == freq_mocking.opendir_callback) {
            return __real_opendir(dirname);
        } else {
            return freq_mocking.opendir_callback(dirname);
        }
    }

    int __wrap_closedir(DIR* dir_fd)
    {
        if(NULL == freq_mocking.closedir_callback) {
            return __real_closedir(dir_fd);
        } else {
            return freq_mocking.closedir_callback(dir_fd);
        }
    }

    struct dirent* __wrap_readdir(DIR* dir_fd)
    {
        if(NULL == freq_mocking.readdir_callback) {
            return __real_readdir(dir_fd);
        } else {
            return freq_mocking.readdir_callback(dir_fd);
        }
    }

} // extern "C"

freq_tests_mocking::freq_tests_mocking() :
    opendir_callback(NULL), closedir_callback(NULL), readdir_callback(NULL)
{
}

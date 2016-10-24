/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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
            return NULL;
        } else {
            return freq_mocking.opendir_callback(dirname);
        }
    }

    FILE* __wrap_fopen(const char * filename, const char * mode)
    {
        if(NULL == freq_mocking.fopen_callback) {
             return NULL;
        } else
            return freq_mocking.fopen_callback(filename, mode);
    }

    int __wrap_fclose(FILE* fd)
    {
        if(NULL == freq_mocking.fclose_callback) {
            return -1;
        } else {
            return freq_mocking.fclose_callback(fd);
        }
    }

    char*__wrap_fgets(char* s , int size, FILE* fd)
    {
        if(NULL == freq_mocking.fgets_callback) {
            return NULL;
        } else {
            return freq_mocking.fgets_callback(s, size, fd);
        }
    }

    int __wrap_closedir(DIR* dir_fd)
    {
        if(NULL == freq_mocking.closedir_callback) {
            return 0;
        } else {
            return freq_mocking.closedir_callback(dir_fd);
        }
    }

    struct dirent* __wrap_readdir(DIR* dir_fd)
    {
        if(NULL == freq_mocking.readdir_callback) {
            return NULL;
        } else {
            return freq_mocking.readdir_callback(dir_fd);
        }
    }

} // extern "C"

freq_tests_mocking::freq_tests_mocking() :
    opendir_callback(NULL), closedir_callback(NULL), fgets_callback(NULL), readdir_callback(NULL), fopen_callback(NULL), fclose_callback(NULL)
{
}

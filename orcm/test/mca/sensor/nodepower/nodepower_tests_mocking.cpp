/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "nodepower_tests_mocking.h"

#include <iostream>
using namespace std;

nodepower_tests_mocking nodepower_mocking;


extern "C" { // Mocking must use correct "C" linkages.
    int __wrap_ipmi_cmdraw(unsigned char a,unsigned char b,unsigned char c,
                           unsigned char d,unsigned char e,unsigned char* f,int g,
                           unsigned char* h,int* i,unsigned char* j,unsigned char k)
    {
        if(NULL == nodepower_mocking.ipmi_cmdraw_callback) {
            return __real_ipmi_cmdraw(a,b,c,d,e,f,g,h,i,j,k);
        } else {
            return nodepower_mocking.ipmi_cmdraw_callback(a,b,c,d,e,f,g,h,i,j,k);
        }
    }

    int __wrap_ipmi_close(void)
    {
        if(NULL == nodepower_mocking.ipmi_close_callback) {
            return __real_ipmi_close();
        } else {
            return nodepower_mocking.ipmi_close_callback();
        }
    }

    __uid_t __wrap_geteuid(void)
    {
        if(NULL == nodepower_mocking.geteuid_callback) {
            return __real_geteuid();
        } else {
            return nodepower_mocking.geteuid_callback();
        }
    }

    struct tm* __wrap_localtime(const time_t* tp)
    {
        if(NULL == nodepower_mocking.localtime_callback) {
            return __real_localtime(tp);
        } else {
            return nodepower_mocking.localtime_callback(tp);
        }
    }
} // extern "C"

nodepower_tests_mocking::nodepower_tests_mocking()
    : ipmi_cmdraw_callback(NULL), ipmi_close_callback(NULL), geteuid_callback(NULL),
      localtime_callback(NULL)
{
}

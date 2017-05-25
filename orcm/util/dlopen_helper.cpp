/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <dlfcn.h>
#include "orcm/util/dlopen_helper.h"

using namespace std;

DlopenHelper::DlopenHelper(string library) : _handler(NULL)
{
    _handler = dlopen(library.c_str(), RTLD_LAZY);
}

DlopenHelper::DlopenHelper(void) : _handler(NULL){}

DlopenHelper::~DlopenHelper(void)
{
    if (_handler)
        dlclose(_handler);
}

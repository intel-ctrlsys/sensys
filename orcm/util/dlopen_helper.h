/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef DLOPEN_HELPER_H
#define DLOPEN_HELPER_H

#include <string>

class DlopenHelper
{
public:
    DlopenHelper(std::string library);
    virtual ~DlopenHelper(void);

protected:
    void* _handler;
    DlopenHelper(void);
};

#endif // DLOPEN_HELPER_H

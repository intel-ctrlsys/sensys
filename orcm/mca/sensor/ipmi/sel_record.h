/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_SEL_RECORD_H
#define ORCM_SEL_RECORD_H

#include <string>

#include <stdint.h>

class sel_record
{
    public: // API
        sel_record(const unsigned char* buffer);
        ~sel_record();

        const char* get_decoded_string() const;

    private:
        std::string decoded_string_;
};

#endif // ORCM_SEL_RECORD_H

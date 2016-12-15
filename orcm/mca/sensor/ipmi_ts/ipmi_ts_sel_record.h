/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_TS_SEL_RECORD_H
#define IPMI_TS_SEL_RECORD_H

#include <string>

#include <stdint.h>

class ipmi_ts_sel_record
{
    public: // API
        ipmi_ts_sel_record(const unsigned char* buffer);
        ~ipmi_ts_sel_record();

        const char* get_decoded_string() const;

    private:
        std::string decoded_string_;
};

#endif

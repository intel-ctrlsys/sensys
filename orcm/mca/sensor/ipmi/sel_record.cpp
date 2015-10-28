/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "sel_record.h"

extern "C" {
    #include <stdio.h>
    #include <string.h>
    #include <ipmicmd.h>
    #include <../share/ipmiutil/ievents.h>
}


//////////////////////////////////////////////////////////////////////////////
// Public API
sel_record::sel_record(const unsigned char* buffer) : decoded_string_("")
{
    char tmp[128]; // More than enough for one line...
    uchar* real_data = (uchar*)&buffer[2];
    decode_sel_entry(real_data, (char*)tmp, 128);
    while(strlen(tmp) > 0 && '\n' == tmp[strlen(tmp)-1]) {
        tmp[strlen(tmp)-1] = '\0';
    }
    decoded_string_ = (const char*)tmp;
}

sel_record::~sel_record()
{
}

const char* sel_record::get_decoded_string() const
{
    return decoded_string_.c_str();
}

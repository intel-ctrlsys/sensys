/*
 * Copyright (c) 2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_credentials.h"


//////////////////////////////////////////////////////////////////////////////
// Public API
ipmi_credentials::ipmi_credentials(const char* bmc_ip, const char* username, const char* password)
    : ip_(bmc_ip), username_(username), password_(password)
{
}

const char* ipmi_credentials::get_bmc_ip() const
{
    return ip_.c_str();
}

const char* ipmi_credentials::get_username() const
{
    return username_.c_str();
}

const char* ipmi_credentials::get_password() const
{
    return password_.c_str();
}

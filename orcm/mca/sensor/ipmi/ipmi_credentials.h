/*
 * Copyright (c) 2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_CREDENTIALS_H
#define IPMI_CREDENTIALS_H

#include <string>

class ipmi_credentials
{
    public:
        ipmi_credentials(const char* bmc_ip, const char* username, const char* password);

        const char* get_bmc_ip() const;
        const char* get_username() const;
        const char* get_password() const;

    private:
        std::string ip_;
        std::string username_;
        std::string password_;
};

#endif // IPMI_CREDENTIALS_H

/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_COLLECTOR_H
#define IPMI_COLLECTOR_H

#define DEFAULT_AUTH_METHOD PASSWORD
#define DEFAULT_PRIV_LEVEL  USER
#define DEFAULT_PORT        1024
#define DEFAULT_CHANNEL     0

#include <string>
#include "orcm/constants.h"

enum auth_methods {
    NONE = 0,
    MD2,
    MD5,
    UNUSED,
    PASSWORD,
    AUTH_OEM
};

enum priv_levels {
    CALLBACK = 1,
    USER,
    OPERATOR,
    ADMIN,
    OEM,
};

/**
 * DISCLAIMER: even when port and channel information is retrieved and
 * handled by the configuration parser, neither IPMI sensor plugin nor
 * chassis-id feature supports a custom configuration for these values.
 * The implementation is not removed in order to be used in the near future.
 */

class ipmiCollector {
    public:
        ipmiCollector();
        ipmiCollector(std::string hostname, std::string bmc_address, std::string aggregator,
                      std::string user, std::string pass);
        ipmiCollector(std::string hostname, std::string bmc_address, std::string aggregator,
                      std::string user, std::string pass, auth_methods auth_method,
                      priv_levels priv_level, int port, int channel);
        ~ipmiCollector() {};

        std::string getHostname()   { return hostname; };
        std::string getBmcAddress() { return bmc_address; };
        std::string getUser()       { return user; };
        std::string getPass()       { return pass; };
        std::string getAggregator() { return aggregator; };
        auth_methods getAuthMethod()    { return auth_method; };
        priv_levels  getPrivLevel()     { return priv_level; };
        int getPort()          { return port; };
        int getChannel()       { return channel; };

        int setAuthMethod(auth_methods auth_method);
        int setPrivLevel(priv_levels priv_level);
        int setPort(int port);
        int setChannel(int channel);

    private:
        std::string bmc_address;
        std::string user;
        std::string pass;
        std::string aggregator;
        std::string hostname;
        auth_methods auth_method;
        priv_levels  priv_level;
        int port;
        int channel;

        void setDefaults();
};

#endif //IPMI_COLLECTOR_H

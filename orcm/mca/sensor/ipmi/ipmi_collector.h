/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
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

using namespace std;

class ipmiCollector {
    public:
        ipmiCollector();
        ipmiCollector(string hostname, string bmc_address, string aggregator,
                      string user, string pass);
        ipmiCollector(string hostname, string bmc_address, string aggregator,
                      string user, string pass, auth_methods auth_method,
                      priv_levels priv_level, int port, int channel);
        ~ipmiCollector() {};

        string getHostname()   { return hostname; };
        string getBmcAddress() { return bmc_address; };
        string getUser()       { return user; };
        string getPass()       { return pass; };
        string getAggregator() { return aggregator; };
        auth_methods getAuthMethod()    { return auth_method; };
        priv_levels  getPrivLevel()     { return priv_level; };
        int getPort()          { return port; };
        int getChannel()       { return channel; };

        int setAuthMethod(auth_methods auth_method);
        int setPrivLevel(priv_levels priv_level);
        int setPort(int port);
        int setChannel(int channel);

    private:
        string bmc_address;
        string user;
        string pass;
        string aggregator;
        string hostname;
        auth_methods auth_method;
        priv_levels  priv_level;
        int port;
        int channel;

        void setDefaults();
};

#endif //IPMI_COLLECTOR_H

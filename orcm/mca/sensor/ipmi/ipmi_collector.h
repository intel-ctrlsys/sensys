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

#define DEFAULT_AUTH_METHOD 4
#define DEFAULT_PRIV_LEVEL  2
#define DEFAULT_PORT        0
#define DEFAULT_CHANNEL     0

#include <string>
#include "orcm/constants.h"

using namespace std;

class ipmiCollector {
    public:
        ipmiCollector();
        ipmiCollector(string hostname, string bmc_address, string aggregator,
                      string user, string pass);
        ipmiCollector(string hostname, string bmc_address, string aggregator,
                      string user, string pass, int auth_method, int priv_level,
                      int port, int channel);
        ~ipmiCollector() {};

        string getHostname()   { return hostname; };
        string getBmcAddress() { return bmc_address; };
        string getUser()       { return user; };
        string getPass()       { return pass; };
        string getAggregator() { return aggregator; };
        int getAuthMethod()    { return auth_method; };
        int getPrivLevel()     { return priv_level; };
        int getPort()          { return port; };
        int getChannel()       { return channel; };

        int setAuthMethod(int auth_method);
        int setPrivLevel(int priv_level);
        int setPort(int port);
        int setChannel(int channel);

    private:
        string bmc_address;
        string user;
        string pass;
        string aggregator;
        string hostname;
        int auth_method;
        int priv_level;
        int port;
        int channel;

        void setDefaults();
};

#endif //IPMI_COLLECTOR_H

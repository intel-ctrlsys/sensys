/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_PARSER_H
#define IPMI_PARSER_H

#include <string>
#include <vector>
#include <map>
#include "orcm/constants.h"
#include "orcm/mca/sensor/ipmi/ipmi_collector.h"
#include "orcm/runtime/orcm_globals.h"

extern "C" {
    #include "orcm/util/utils.h"
    #include "opal/mca/installdirs/installdirs.h"
}

#define DEFAULT_FILE_NAME "ipmi.xml"
#define XML_IPMI "ipmi"
#define XML_BMC_NODE "bmc_node"
#define XML_NAME "name"
#define XML_BMC_ADDRESS "bmc_address"
#define XML_USER "user"
#define XML_PASS "pass"
#define XML_AGGREGATOR "aggregator"
#define XML_AUTH_METHOD "auth_method"
#define XML_PRIV_LEVEL "priv_level"
#define XML_PORT "port"
#define XML_CHANNEL "channel"

typedef map<string, ipmiCollector> ipmiCollectorMap;
typedef vector<ipmiCollector> ipmiCollectorVector;

using namespace std;

class ipmiParser {
    public:
        ipmiParser(const string& file="");
        ~ipmiParser() {};

        ipmiCollectorMap    getIpmiCollectorMap() { return ipmiMap; };
        ipmiCollectorVector getIpmiCollectorVector() { return ipmiVector; };

    private:
        string file;
        int fileId;
        string IPMI_DEFAULT_FILE_PATH;
        ipmiCollectorMap ipmiMap;
        ipmiCollectorVector ipmiVector;

        int  openFile();
        void closeFile();
        void setFile(const string& file);
        void setDefaultPath();
        void parse();
        void getIpmiCollectorMapFromIpmiSections(opal_list_t *ipmiSections);
        void fillVectorFromMap();
        bool itemListHasChildren(orcm_value_t *item);
        bool fieldsAreNotEmpty(string hostname, string bmcAddress,
                               string aggregator, string user, string pass);
        auth_methods getAuthMethodType(char *auth_method);
        priv_levels  getPrivLevelType(char *priv_level);
        ipmiCollectorMap getIpmiCollectorMapFromBmcNodes(opal_list_t *bmcNodes);
        void unique_map_join(ipmiCollectorMap &main_map,
                             ipmiCollectorMap join_map);
        ipmiCollector* getIpmiCollectorFromOrcmValue(orcm_value_t *node);
        ipmiCollector* buildIpmiCollectorFromList(opal_list_t* list);
        void getAllIpmiValues(string& hostname, string& bmcAddress, string& user,
                      string& pass, string& aggregator, auth_methods& authMethod,
                      priv_levels& privLevel, int& port, int& channel, opal_list_t*list);
};

#endif //IPMI_PARSER_H

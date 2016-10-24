/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/runtime/orcm_globals.h"

extern "C" {
    #include "orcm/util/utils.h"
    #include "opal/mca/installdirs/installdirs.h"
}

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

typedef std::map<std::string, ipmiCollector> ipmiCollectorMap;
typedef std::vector<ipmiCollector> ipmiCollectorVector;

ORCM_DECLSPEC extern orcm_cfgi_base_t orcm_cfgi_base;

/**
 * DISCLAIMER: even when port and channel information is retrieved and
 * handled by the configuration parser, neither IPMI sensor plugin nor
 * chassis-id feature supports a custom configuration for these values.
 * The implementation is not removed in order to be used in the near future.
 */

class ipmiParser {
    public:
        ipmiParser(const std::string& file="");
        ~ipmiParser() { closeFile(); };

        ipmiCollectorMap    getIpmiCollectorMap() { return ipmiMap; };
        ipmiCollectorVector getIpmiCollectorVector() { return ipmiVector; };

    private:
        std::string file;
        int fileId;
        ipmiCollectorMap ipmiMap;
        ipmiCollectorVector ipmiVector;

        int  openFile();
        void closeFile();
        void setFile(const std::string& file);
        void parse();
        void getIpmiCollectorMapFromIpmiSections(opal_list_t *ipmiSections);
        void fillVectorFromMap();
        bool itemListHasChildren(orcm_value_t *item);
        bool fieldsAreNotEmpty(std::string hostname, std::string bmcAddress,
                               std::string aggregator, std::string user, std::string pass);
        auth_methods getAuthMethodType(char *auth_method);
        priv_levels  getPrivLevelType(char *priv_level);
        ipmiCollectorMap getIpmiCollectorMapFromBmcNodes(opal_list_t *bmcNodes);
        void unique_map_join(ipmiCollectorMap &main_map,
                             ipmiCollectorMap join_map);
        ipmiCollector* getIpmiCollectorFromOrcmValue(orcm_value_t *node);
        ipmiCollector* buildIpmiCollectorFromList(opal_list_t* list);
        void getAllIpmiValues(std::string& hostname, std::string& bmcAddress, std::string& user,
                              std::string& pass, std::string& aggregator, auth_methods& authMethod,
                              priv_levels& privLevel, int& port, int& channel, opal_list_t*list);
};

#endif //IPMI_PARSER_H

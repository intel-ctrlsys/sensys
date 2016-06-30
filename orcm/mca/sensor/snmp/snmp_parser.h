/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef SNMP_PARSER_H
#define SNMP_PARSER_H

#include <string>
#include <vector>
#include <map>
#include "orcm/constants.h"
#include "snmp_collector.h"
#include "orcm/util/string_utils.h"
#include "orcm/runtime/orcm_globals.h"

extern "C" {
    #include "orcm/util/utils.h"
    #include "opal/mca/installdirs/installdirs.h"
    #include "opal/util/argv.h"
    #include "orcm/util/logical_group.h"
    #include "orcm/mca/parser/parser.h"
}

#define DEFAULT_SEC_TYPE  AUTHNOPRIV
#define DEFAULT_AUTH_TYPE MD5
#define DEFAULT_PRIV_TYPE DES
#define SNMP_DEFAULT_FILE_NAME "orcm-default-config.xml"

#define XML_SNMP "snmp"
#define XML_CONFIG_NODE "config"
#define XML_NAME "name"
#define XML_VERSION "version"
#define XML_USER "user"
#define XML_PASS "pass"
#define XML_AUTH_TYPE "auth"
#define XML_SEC_TYPE "sec"
#define XML_LOCATION "location"
#define XML_AGGREGATOR "aggregator"
#define XML_HOSTNAME "hostname"
#define XML_OIDS "oids"
#define XML_PRIV_TYPE "priv"

typedef std::map<std::string, snmpCollector> snmpCollectorMap;
typedef std::vector<snmpCollector> snmpCollectorVector;

using std::runtime_error;

class snmpParser {
    public:
        snmpParser(const std::string& filePath="");
        ~snmpParser();
        void setParseFile(const std::string& filePath="");
        void unsetParseFile();
        snmpCollectorVector getSnmpCollectorVector();

    private:
        std::string SNMP_DEFAULT_FILE_PATH;
        std::string file;
        int fileId;
        snmpCollectorMap snmpMap;
        snmpCollectorVector collectors, snmpVector;

        void setDefaultPath();
        void parseFile();
        void openConfigFile();
        void closeConfigFile();
        void fillVectorFromMap();
        void getSnmpCollectorFromSnmpSections(opal_list_t *snmpSections);
        bool itemListHasChildren(orcm_value_t *item);
        void unique_map_join(snmpCollectorMap &main_map, snmpCollectorMap join_map);
        snmpCollectorMap getSnmpCollectorMapFromConfigNodes(opal_list_t *configNodes);
        void addSnmpCollectorsFromOrcmValueToMap(orcm_value_t *node, snmpCollectorMap &tmpMap);
        void buildSnmpCollectorsFromList(opal_list_t* list);
        void getAllSnmpValues(std::string& aggregator, std::string& hostname, std::string& version,
                              std::string& user, std::string& pass, std::string& location, std::string& oids,
                              auth_type& authType, sec_type& secType, priv_protocol& priv, opal_list_t* list);
        auth_type getAuthType(char *authType);
        sec_type getSecType(char *secType);
        priv_protocol getPrivProtocol(char *priv);
        inline bool fieldsAreNotEmpty(std::string aggregator, std::string hostname, std::string version,
                                      std::string user, std::string oids);
        bool aggregatorIsThisHostname(std::string aggregator);
        void getSnmpCollectors(std::string version, std::string  hostname, std::string user, std::string pass,
                               auth_type authType, sec_type secType, priv_protocol priv, std::string oids, std::string location);
        std::vector<std::string> expandLogicalGroup(std::string str);
        snmpCollector getSnmpCollectorVersion1(std::string hostname, std::string user,
                                               std::string oids, std::string location);
        snmpCollector getSnmpCollectorVersion3(std::string hostname, std::string user, std::string pass,
                                               auth_type auth, sec_type sec, priv_protocol priv, std::string oids, std::string location);
};

class fileNotFound : public runtime_error {
    public:
        fileNotFound() : runtime_error( "SNMP configuration file was not found!" ) {}
};

#endif /* SNMP_PARSER_H */

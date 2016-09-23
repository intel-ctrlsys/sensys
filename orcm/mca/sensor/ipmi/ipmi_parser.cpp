/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmi_parser.h"
#include "orcm/mca/parser/parser.h"
#include <string.h>
#include <regex.h>

#define IP_ADDRESS_REGEX \
    "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])[.]){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"

#define getStringValueFromItem(variable, searchedKey, field) \
    if (NULL != field && OPAL_STRING == field->value.type    \
        && 0 == strcmp(searchedKey, field->value.key)){      \
        variable = string(field->value.data.string);         \
    }

#define getIntValueFromItem(variable, searchedKey, field) \
    if (NULL != field && OPAL_STRING == field->value.type \
        && 0 == strcmp(searchedKey, field->value.key)){   \
        variable = atoi(field->value.data.string);        \
    }

#define getAuthMethodFromItem(variable, searchedKey, field)     \
    if (NULL != field && OPAL_STRING == field->value.type       \
        && 0 == strcmp(searchedKey, field->value.key)){         \
        variable = getAuthMethodType(field->value.data.string); \
    }

#define getPrivLevelFromItem(variable, searchedKey, field)     \
    if (NULL != field && OPAL_STRING == field->value.type      \
        && 0 == strcmp(searchedKey, field->value.key)){        \
        variable = getPrivLevelType(field->value.data.string); \
    }

#define returnIfMatches(variable, pattern, value) \
    if (NULL != variable && stringMatchRegex(string(variable), pattern)){ \
        return value;                                                     \
    }

using namespace std;

static inline bool stringMatchRegex(string str, string pattern){
   regex_t regex_comp;
   regcomp(&regex_comp, pattern.c_str(), REG_ICASE|REG_EXTENDED);
   bool rv = !regexec(&regex_comp, str.c_str(), 0, NULL, 0);
   regfree(&regex_comp);
   return rv;
}

ipmiParser::ipmiParser(const string& file)
{
   fileId = ORCM_ERROR;
   setFile(file);
   parse();
}

void ipmiParser::setFile(const string& file)
{
    if (file.empty()){
        if( 3 < orcm_cfgi_base.version ) {
            this->file = orcm_cfgi_base.config_file;
        } else {
            string prefix = (NULL != opal_install_dirs.prefix)? string(opal_install_dirs.prefix) : "";
            this->file = prefix + string("/etc/") + string("orcm-default-config.xml");
        }
    } else {
        this->file = file;
    }
}

void ipmiParser::parse()
{
    if (ORCM_ERROR != openFile()){
        opal_list_t *ipmiSections = orcm_parser.retrieve_section(fileId, XML_IPMI, NULL);
        getIpmiCollectorMapFromIpmiSections(ipmiSections);
        SAFE_RELEASE_NESTED_LIST(ipmiSections);
        fillVectorFromMap();
        closeFile();
    }
}

int ipmiParser::openFile()
{
    int fileId = orcm_parser.open(file.c_str());
    if (0 < fileId){
        this->fileId = fileId;
        return ORCM_SUCCESS;
    }
    return ORCM_ERROR;
}

void ipmiParser::getIpmiCollectorMapFromIpmiSections(opal_list_t *ipmiSections)
{
    if (NULL != ipmiSections){
        orcm_value_t *section = NULL;
        OPAL_LIST_FOREACH(section, ipmiSections, orcm_value_t){
            if (itemListHasChildren(section)){
                opal_list_t *bmcNodes = orcm_parser.retrieve_section_from_list(fileId,
                                   (opal_list_item_t*)section, XML_BMC_NODE, NULL);
                unique_map_join(ipmiMap, getIpmiCollectorMapFromBmcNodes(bmcNodes));
                SAFE_RELEASE_NESTED_LIST(bmcNodes);
            }
        }
    }
}

bool  ipmiParser::itemListHasChildren(orcm_value_t *item)
{
    if (NULL != item && OPAL_PTR == item->value.type){
        return true;
    }
    return false;
}

void ipmiParser::unique_map_join(ipmiCollectorMap &main_map,
                                             ipmiCollectorMap join_map)
{
    for (ipmiCollectorMap::iterator it=join_map.begin(); it != join_map.end(); ++it){
        if (main_map.find(it->first) == main_map.end()){
             main_map.insert(*it);
        }
    }
}

ipmiCollectorMap ipmiParser::getIpmiCollectorMapFromBmcNodes(opal_list_t *bmcNodes)
{
    ipmiCollectorMap tmpMap;
    if (NULL != bmcNodes){
        orcm_value_t *node = NULL;
        OPAL_LIST_FOREACH(node, bmcNodes, orcm_value_t){
            ipmiCollector *ic = getIpmiCollectorFromOrcmValue(node);
            if (NULL != ic){
                tmpMap[ic->getHostname()]=*ic;
                delete ic;
            }
        }
    }
    return tmpMap;
}

ipmiCollector* ipmiParser::getIpmiCollectorFromOrcmValue(orcm_value_t *node)
{
    ipmiCollector *collector = NULL;
    if (itemListHasChildren(node)){
        collector = buildIpmiCollectorFromList((opal_list_t*)node->value.data.ptr);
    }
    return collector;
}

ipmiCollector* ipmiParser::buildIpmiCollectorFromList(opal_list_t* list)
{
    if (NULL == list){
        return NULL;
    }

    ipmiCollector *collector = NULL;
    string hostname = "", bmcAddress = "", user = "", pass = "", aggregator = "";
    auth_methods authMethod = DEFAULT_AUTH_METHOD;
    priv_levels privLevel = DEFAULT_PRIV_LEVEL;
    int port = ORCM_ERROR, channel = ORCM_ERROR;

    getAllIpmiValues(hostname, bmcAddress, user, pass, aggregator,
                     authMethod, privLevel, port, channel, list);

    if (fieldsAreNotEmpty(hostname, bmcAddress, aggregator, user, pass)){
        if (stringMatchRegex(bmcAddress, IP_ADDRESS_REGEX)){
            collector = new ipmiCollector(hostname, bmcAddress, aggregator, user, pass,
                                          authMethod, privLevel, port, channel);
        }
    }
    return collector;
}

void ipmiParser::getAllIpmiValues(string& hostname, string& bmcAddress, string& user,
                                  string& pass, string& aggregator, auth_methods& authMethod,
                                  priv_levels& privLevel, int& port, int& channel, opal_list_t* list)
{
    orcm_value_t *field = NULL;
    OPAL_LIST_FOREACH(field, list, orcm_value_t){
        getStringValueFromItem(hostname, XML_NAME, field);
        getStringValueFromItem(bmcAddress, XML_BMC_ADDRESS, field);
        getStringValueFromItem(user, XML_USER, field);
        getStringValueFromItem(pass, XML_PASS, field);
        getStringValueFromItem(aggregator, XML_AGGREGATOR, field);
        getAuthMethodFromItem(authMethod, XML_AUTH_METHOD, field);
        getPrivLevelFromItem(privLevel, XML_PRIV_LEVEL, field);
        getIntValueFromItem(port, XML_PORT, field);
        getIntValueFromItem(channel, XML_CHANNEL, field);
    }
}

auth_methods ipmiParser::getAuthMethodType(char *auth_method)
{
    if (NULL != auth_method && 0 != strcmp("", auth_method)){
        returnIfMatches(auth_method, "NONE", NONE);
        returnIfMatches(auth_method, "MD2", MD2);
        returnIfMatches(auth_method, "MD5", MD5);
        returnIfMatches(auth_method, "PASSWORD", PASSWORD);
        returnIfMatches(auth_method, "OEM", AUTH_OEM);
    }
    return DEFAULT_AUTH_METHOD;
}

priv_levels ipmiParser::getPrivLevelType(char *priv_level)
{
    if (NULL != priv_level && 0 != strcmp("", priv_level)){
        returnIfMatches(priv_level, "CALLBACK", CALLBACK);
        returnIfMatches(priv_level, "USER", USER);
        returnIfMatches(priv_level, "OPERATOR", OPERATOR);
        returnIfMatches(priv_level, "ADMIN", ADMIN);
        returnIfMatches(priv_level, "OEM", OEM);
    }
    return DEFAULT_PRIV_LEVEL;
}

bool ipmiParser::fieldsAreNotEmpty(string hostname, string bmcAddress,
                                   string aggregator, string user, string pass)
{
    if (!hostname.empty() && !bmcAddress.empty() && !aggregator.empty() &&
        !user.empty() && !pass.empty()){
        return true;
    }
    return false;
}

void ipmiParser::fillVectorFromMap()
{
    for (ipmiCollectorMap::iterator it=ipmiMap.begin(); it != ipmiMap.end(); ++it){
        ipmiVector.push_back(it->second);
    }
}

void ipmiParser::closeFile()
{
    if (0 < fileId){
        orcm_parser.close(fileId);
        fileId = ORCM_ERROR;
    }
}

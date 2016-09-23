/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_parser.h"
#include "orcm/runtime/orcm_globals.h"

#define getStringValueFromItem(variable, searchedKey, field) \
    if (NULL != field && OPAL_STRING == field->value.type    \
        && 0 == strcmp(searchedKey, field->value.key)){      \
        variable = string(field->value.data.string);         \
    }

#define getAuthTypeFromItem(variable, searchedKey, field)  \
    if (NULL != field && OPAL_STRING == field->value.type  \
        && 0 == strcmp(searchedKey, field->value.key)){    \
        variable = getAuthType(field->value.data.string);  \
    }

#define getSecTypeFromItem(variable, searchedKey, field)   \
    if (NULL != field && OPAL_STRING == field->value.type  \
        && 0 == strcmp(searchedKey, field->value.key)){    \
        variable = getSecType(field->value.data.string);   \
    }

#define getPrivProtocolFromItem(variable, searchedKey, field) \
    if (NULL != field && OPAL_STRING == field->value.type     \
        && 0 == strcmp(searchedKey, field->value.key)){       \
        variable = getPrivProtocol(field->value.data.string); \
    }

#define returnIfMatches(variable, pattern, value) \
    if (NULL != variable && stringMatchRegex(string(variable), pattern)){ \
        return value;                                                     \
    }

using namespace std;


snmpParser::snmpParser(const string& filePath)
{
    fileId = ORCM_ERROR;
    setParseFile(filePath);
}

snmpParser::~snmpParser()
{
    unsetParseFile();
}

/**
 * @brief Function that sets the file to be parsed. If no file is provided the default will be set.
 *
 * @param filePath Path to the file (including the full file name) to be parsed.
 *
 */
void snmpParser::setParseFile(const string& filePath)
{
    unsetParseFile();
    if( filePath.empty()){
        if( 3 < orcm_cfgi_base.version ) {
            this->file = orcm_cfgi_base.config_file;
        } else {
            string prefix = (NULL != opal_install_dirs.prefix)? string(opal_install_dirs.prefix) : "";
            this->file = prefix + string("/etc/") + string("orcm-default-config.xml");
        }
    } else {
        file = filePath;
    }
    parseFile();
}

/**
 * @brief Function that unsets the file to be parsed (if its loaded).
 *
 */
void snmpParser::unsetParseFile()
{
    closeConfigFile();
    file = "";
    snmpMap.clear();
    snmpVector.clear();
}


/**
 * @brief Function that returns a vector with the SNMP objects to be monitored by this host
 *        taken from the SNMP Configuration File.
 *
 * @return Vector with the SNMP devices (and its configuration) to be monitored by this host.
 */
snmpCollectorVector snmpParser::getSnmpCollectorVector()
{
    return snmpVector;
}

void snmpParser::parseFile()
{
    openConfigFile();
    opal_list_t *snmpSections = orcm_parser.retrieve_section(fileId, XML_SNMP, NULL);
    getSnmpCollectorFromSnmpSections(snmpSections);
    SAFE_RELEASE_NESTED_LIST(snmpSections);
    closeConfigFile();
    fillVectorFromMap();
}

void snmpParser::openConfigFile()
{
    int fileId = orcm_parser.open(file.c_str());
    if(0 < fileId ){
        this->fileId = fileId;
    } else {
        throw fileNotFound();
    }
}

void snmpParser::closeConfigFile()
{
    if(0 < fileId ){
        orcm_parser.close(fileId);
        fileId = ORCM_ERROR;
    }
}

void snmpParser::getSnmpCollectorFromSnmpSections(opal_list_t *snmpSections)
{
    if (NULL != snmpSections){
        orcm_value_t *section = NULL;
        OPAL_LIST_FOREACH(section, snmpSections, orcm_value_t){
            if (itemListHasChildren(section)){
                opal_list_t *configNodes = orcm_parser.retrieve_section_from_list(fileId,
                                   (opal_list_item_t*)section, XML_CONFIG_NODE, NULL);
                unique_map_join(snmpMap, getSnmpCollectorMapFromConfigNodes(configNodes));
                SAFE_RELEASE_NESTED_LIST(configNodes);
            }
        }
    }
}

bool  snmpParser::itemListHasChildren(orcm_value_t *item)
{
    if (NULL != item && OPAL_PTR == item->value.type){
        return true;
    }
    return false;
}

void snmpParser::unique_map_join(snmpCollectorMap &main_map,
                                             snmpCollectorMap join_map)
{
    for (snmpCollectorMap::iterator it=join_map.begin(); it != join_map.end(); ++it){
        if (main_map.find(it->first) == main_map.end()){
             main_map.insert(*it);
        }
    }
}

snmpCollectorMap snmpParser::getSnmpCollectorMapFromConfigNodes(opal_list_t *configNodes)
{
    snmpCollectorMap tmpMap;
    if (NULL != configNodes){
        orcm_value_t *node = NULL;
        OPAL_LIST_FOREACH(node, configNodes, orcm_value_t){
            addSnmpCollectorsFromOrcmValueToMap(node, tmpMap);
        }
    }
    return tmpMap;
}

void  snmpParser::addSnmpCollectorsFromOrcmValueToMap(orcm_value_t *node, snmpCollectorMap &tmpMap)
{
    if (itemListHasChildren(node)){
        collectors.clear();
        buildSnmpCollectorsFromList((opal_list_t*)node->value.data.ptr);
        for (snmpCollectorVector::iterator it=collectors.begin(); it != collectors.end(); it++){
            tmpMap[it->getHostname()]=*it;
        }
    }
}

void snmpParser::buildSnmpCollectorsFromList(opal_list_t* list)
{
    string hostname = "",  user = "", pass = "", aggregator = "", location= "";
    string version="", oids="";
    auth_type authType = DEFAULT_AUTH_TYPE;
    sec_type secType = DEFAULT_SEC_TYPE;
    priv_protocol priv = DEFAULT_PRIV_TYPE;

    getAllSnmpValues(aggregator, hostname, version, user, pass, location, oids,
                     authType, secType, priv, list);

    if (fieldsAreNotEmpty(aggregator, hostname, version, user, oids) &&
        aggregatorIsThisHostname(aggregator)){
        getSnmpCollectors(version, hostname, user, pass, authType, secType, priv, oids, location);
    }
}

void snmpParser::getAllSnmpValues(string& aggregator, string& hostname, string& version,
                                  string& user, string& pass, string& location, string& oids,
                                  auth_type& authType, sec_type& secType, priv_protocol& priv, opal_list_t* list)
{
    orcm_value_t *field = NULL;
    OPAL_LIST_FOREACH(field, list, orcm_value_t){
        getStringValueFromItem(aggregator, XML_AGGREGATOR, field);
        getStringValueFromItem(hostname, XML_HOSTNAME, field);
        getStringValueFromItem(version, XML_VERSION, field);
        getStringValueFromItem(user, XML_USER, field);
        getStringValueFromItem(pass, XML_PASS, field);
        getStringValueFromItem(location, XML_LOCATION, field);
        getStringValueFromItem(oids, XML_OIDS, field);
        getAuthTypeFromItem(authType, XML_AUTH_TYPE, field);
        getSecTypeFromItem(secType, XML_SEC_TYPE, field);
        getPrivProtocolFromItem(priv, XML_PRIV_TYPE, field);
    }
}

auth_type snmpParser::getAuthType(char *authType)
{
    if (NULL != authType && 0 != strcmp("", authType)){
        returnIfMatches(authType, "MD5", MD5);
        returnIfMatches(authType, "SHA1", SHA1);
    }
    return DEFAULT_AUTH_TYPE;
}

sec_type snmpParser::getSecType(char *secType)
{
    if (NULL != secType && 0 != strcmp("", secType)){
        returnIfMatches(secType, "NOAUTH", NOAUTH);
        returnIfMatches(secType, "AUTHNOPRIV", AUTHNOPRIV);
        returnIfMatches(secType, "AUTHPRIV", AUTHPRIV);
    }
    return DEFAULT_SEC_TYPE;
}

priv_protocol snmpParser::getPrivProtocol(char *priv)
{
    if (NULL != priv && 0 != strcmp("", priv)){
        returnIfMatches(priv, "NOPRIV", NOPRIV);
        returnIfMatches(priv, "DES", DES);
        returnIfMatches(priv, "AES", AES);
    }
    return DEFAULT_PRIV_TYPE;
}


inline bool snmpParser::fieldsAreNotEmpty(string aggregator, string hostname,
                                   string version, string user, string oids)
{
    return !(hostname.empty() || aggregator.empty() || version.empty() ||
        user.empty() || oids.empty());
}

bool snmpParser::aggregatorIsThisHostname(string aggregator){
    char* thisHostname = orcm_get_proc_hostname();
    if (NULL == thisHostname) {
        thisHostname = (char*) "localhost";
    }

    if (0 == aggregator.compare(thisHostname) || 0 == aggregator.compare("localhost")){
        return true;
    }
    return false;
}

void snmpParser::getSnmpCollectors(string version, string  hostname, string user, string pass,
                                   auth_type authType, sec_type secType, priv_protocol priv, string oids, string location)
{
    vector<string> hostnames = expandLogicalGroup(hostname);
    for (vector<string>::iterator host = hostnames.begin(); host != hostnames.end() ; host++){
        if (0 == version.compare("1")){
            collectors.push_back(getSnmpCollectorVersion1(*host, user, oids, location));
        } else if (0 == version.compare("2") || 0 == version.compare("3")) {
            collectors.push_back(getSnmpCollectorVersion3(*host, user, pass, authType, secType,
                                                           priv, oids, location));
        }
    }
}

vector<string> snmpParser::expandLogicalGroup(string str)
{
    char **hosts = NULL;
    int count = 0;

    if (ORCM_SUCCESS != orcm_logical_group_parse_array_string(const_cast<char*>(str.c_str()), &hosts))
    {
        return vector<string>();
    }

    count = opal_argv_count(hosts);

    vector<string> result(hosts, hosts+count);
    opal_argv_free(hosts);
    return result;
}


snmpCollector snmpParser::getSnmpCollectorVersion1(string hostname, string user, string oids, string location)
{
    snmpCollector collector(hostname, user);
    collector.setLocation(location);
    collector.setOIDs(oids);
    return collector;
}

snmpCollector snmpParser::getSnmpCollectorVersion3(string hostname, string user, string pass, auth_type auth,
                                                   sec_type secType, priv_protocol priv, string oids, string location)
{
    snmpCollector collector(hostname, user, pass, auth, secType, priv);
    collector.setLocation(location);
    collector.setOIDs(oids);
    return collector;
}

void snmpParser::fillVectorFromMap()
{
    for (snmpCollectorMap::iterator it=snmpMap.begin(); it != snmpMap.end(); ++it){
        snmpVector.push_back(it->second);
    }
}

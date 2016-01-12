/*
 * Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <fstream>
#include <iostream>
#include <regex.h>
#include <stdio.h>
#include <string>

#include "snmp_collector.h"
extern "C"{
    #include "opal/util/argv.h"
    #include "opal/mca/installdirs/installdirs.h"
    #include "orcm/util/logical_group.h"
}

#ifndef SNMP_PARSER_H
#define SNMP_PARSER_H

typedef std::map<std::string,snmpCollector> snmpCollectorMap;

enum snmp_version{v1,v3};

using std::runtime_error;

class snmpParser {
    public:
        snmpParser(std::string filePath="");
        ~snmpParser();
        void setParseFile(std::string filePath="");
        void unsetParseFile();
        std::vector<snmpCollector> parse();

    private:
        std::string SNMP_DEFAULT_FILE_PATH;
        std::string _filePath;
        std::ifstream _snmp_file;
        std::string hostname;
        std::string username;
        std::string password;
        int version;
        sec_type sec;
        auth_type auth;

        void openConfigFile();
        void closeConfigFile();
        std::vector<snmpCollector> getSnmpCollectorVector();
        void getHostnameVectorAndSnmpConfigMap(std::vector<std::string> &hostnames_vector, snmpCollectorMap &snmp_config_map);
        void processNodeTag(std::vector<std::string> &hostnames_vector);
        void processSnmpConfigTag(snmpCollectorMap &snmp_config_map);
        std::vector<std::string> getTagEntriesVector();
        std::vector<std::string> getMonitoredHostNamesFromTagEntries(std::vector<std::string> lines);
        snmpCollectorMap getSnmpConfigMapFromTagEntries(std::vector<std::string> lines);
        std::string getValueFromLine(std::string line);
        std::vector<std::string> getVectorOfValuesFromLine(std::string line);
        snmpCollector getSnmpCollectorVersion1(std::string hostname, std::string user, std::string oids, std::string location);
        snmpCollector getSnmpCollectorVersion3(std::string hostname, std::string user, std::string pass, std::string auth, std::string sec, std::string oids, std::string location);
        auth_type getAuthType(std::string auth);
        sec_type getSecType(std::string sec);
        std::vector<std::string> expandLogicalGroup(std::string str);
        bool isSnmpConfigTag(std::string line);
        bool isNodesTag(std::string line);
        bool isTag(std::string line);
        bool isThisHostname(std::string line);
        bool isHostname(std::string line);
        bool isVersion(std::string line);
        bool isUser(std::string line);
        bool isPass(std::string line);
        bool isAuthentication(std::string line);
        bool isSecurity(std::string line);
        bool isOIDs(std::string line);
        bool isLocation(std::string line);
};

class fileNotFound : public runtime_error {
    public:
        fileNotFound() : runtime_error( "SNMP configuration file was not found!" ) {}
};

#endif /* SNMP_PARSER_H */

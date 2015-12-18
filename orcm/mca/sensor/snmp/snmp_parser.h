/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
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

typedef map<string,snmpCollector> snmpCollectorMap;

enum snmp_version{v1,v3};

using std::runtime_error;
using namespace std;

class snmpParser {
    public:
        snmpParser(string filePath="");
        ~snmpParser();
        void setParseFile(string filePath="");
        void unsetParseFile();
        vector<snmpCollector> parse();

    private:
        string SNMP_DEFAULT_FILE_PATH;
        string _filePath;
        ifstream _snmp_file;
        string hostname;
        string username;
        string password;
        int version;
        sec_type sec;
        auth_type auth;

        void openConfigFile();
        void closeConfigFile();
        vector<snmpCollector> getSnmpCollectorVector();
        void getHostnameVectorAndSnmpConfigMap(vector<string> &hostnames_vector, snmpCollectorMap &snmp_config_map);
        void processNodeTag(vector<string> &hostnames_vector);
        void processSnmpConfigTag(snmpCollectorMap &snmp_config_map);
        vector<string> getTagEntriesVector();
        vector<string> getMonitoredHostNamesFromTagEntries(vector<string> lines);
        snmpCollectorMap getSnmpConfigMapFromTagEntries(vector<string> lines);
        string getValueFromLine(string line);
        vector<string> getVectorOfValuesFromLine(string line);
        snmpCollector getSnmpCollectorVersion1(string hostname, string user, string oids, string location);
        snmpCollector getSnmpCollectorVersion3(string hostname, string user, string pass, string auth, string sec, string oids, string location);
        auth_type getAuthType(string auth);
        sec_type getSecType(string sec);
        vector<string> expandLogicalGroup(string str);
        bool isSnmpConfigTag(string line);
        bool isNodesTag(string line);
        bool isTag(string line);
        bool isThisHostname(string line);
        bool isHostname(string line);
        bool isVersion(string line);
        bool isUser(string line);
        bool isPass(string line);
        bool isAuthentication(string line);
        bool isSecurity(string line);
        bool isOIDs(string line);
        bool isLocation(string line);
};

class fileNotFound : public runtime_error {
    public:
        fileNotFound() : runtime_error( "SNMP configuration file was not found!" ) {}
};

#endif /* SNMP_PARSER_H */

/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "net-snmp/library/transform_oids.h"

#include <iostream>
#include <list>
#include <map>
#include "vardata.h"
#include <stdexcept>

#ifndef SNMP_COLLECTOR_H
#define SNMP_COLLECTOR_H

#define STRING_BUFFER_SIZE 1024
#define SUCCESS 0

enum auth_type {MD5, SHA1};
enum sec_type {NOAUTH, AUTHNOPRIV, AUTHPRIV};

using std::runtime_error;
using namespace std;

class snmpCollector {
    public:
        snmpCollector();
        snmpCollector(string hostname, string username);
        snmpCollector(string hostname, string username, string password);
        snmpCollector(string hostname, string username, string password, auth_type auth);
        snmpCollector(string hostname, string username, string password, auth_type auth, sec_type sec);
        ~snmpCollector();

        void dump_session(netsnmp_session *s);
        void dump_pdu(netsnmp_pdu *p);
        void setOIDs(string strOIDs);
        void updateOIDs();
        void setLocation(string location);
        list<string> getRequestedOIDs();
        vector<vardata> collectData();

    private:
        struct snmp_session session;
        netsnmp_pdu *pdu;
        netsnmp_pdu *response;
        oid anOID[MAX_OID_LEN];
        size_t anOID_len;

        string hostname, username, password, location;
        list<string> oidList;

        void setSecurityLevel(sec_type sec);
        void setMD5Authentication(string password);
        void setSHA1Authentication(string password);
        void storeCharsAndLength(string s, char **c_str, size_t *len);
        list<string> splitString(string input, char delimiter);
        vector<vardata> packCollectedData(netsnmp_pdu *response);

        static inline string &ltrim(string &s) {
            s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
            return s;
        }

        static inline string &rtrim(string &s) {
            s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
            return s;
        }

        static inline string &trim(string &s) {
            return ltrim(rtrim(s));
        }
};

class invalidPassword : public runtime_error {
    public:
        invalidPassword() : runtime_error( "Unable to generate encrypted password" ) {}
};

class invalidOIDParsing: public runtime_error {
    public:
        invalidOIDParsing() : runtime_error("Unable to parse OID object or string") {}
};

class invalidSession: public runtime_error {
    public:
        invalidSession() : runtime_error("Unable to create SNMP session") {}
};

class snmpTimeout: public runtime_error {
    public:
        snmpTimeout() : runtime_error("Connection to SNMP device timed out") {}
};

class dataCollectionError: public runtime_error {
    public:
        dataCollectionError() : runtime_error("Error during data collection") {}
};

class packetError: public runtime_error {
    public:
        packetError() : runtime_error("Error in packet") {}
};

#endif /* SNMP_COLLECTOR_H */

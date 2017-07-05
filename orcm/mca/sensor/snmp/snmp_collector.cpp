/*
 * Copyright (c) 2015-2017  Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_collector.h"

#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

#include <arpa/inet.h>

using namespace std;

snmpCollector::snmpCollector() {
    initDataMembers();
    snmpCollector("","");
}

snmpCollector::snmpCollector(string host, string user) {
    initDataMembers();
    hostname = string(host);
    username = string(user);

    init_snmp("orcm");
    snmp_sess_init( &session );
    session.version = SNMP_VERSION_1;
    updateCharPointers();
}

snmpCollector::snmpCollector(string host, string user, string pass) {
    initDataMembers();
    snmpCollector(host, user, pass, MD5);
}

snmpCollector::snmpCollector(string host, string user, string pass, auth_type auth) {
    initDataMembers();
    snmpCollector(host, user, pass, auth, AUTHNOPRIV);
}

snmpCollector::snmpCollector(string host, string user, string pass, auth_type auth, sec_type sec) {
    initDataMembers();
    snmpCollector(host, user, pass, auth, sec, DES);
}


snmpCollector::snmpCollector(string host, string user, string pass, auth_type auth, sec_type sec, priv_protocol priv) {
    initDataMembers();
    hostname = string(host);
    username = string(user);
    password = string(pass);

    init_snmp("orcm");
    snmp_sess_init( &session );
    session.version=SNMP_VERSION_3;
    updateCharPointers();

    setSecurityLevel(sec);
    setPrivacyLevel(priv);

    switch(auth) {
        case MD5:
            setMD5Authentication(password);
            break;
        case SHA1:
            setSHA1Authentication(password);
            break;
    }
}

void snmpCollector::updateCharPointers() {
    session.peername = const_cast<char*>(hostname.c_str());
    storeCharsAndLength(username, (char**) &session.community, &session.community_len);
    storeCharsAndLength(username, &session.securityName, &session.securityNameLen);
}

void snmpCollector::initDataMembers() {
    anOID_len = 0;
    pdu = NULL;
    response = NULL;
    runtime_metrics_ = NULL;
    session = {0};
}

snmpCollector::~snmpCollector() {
}

void snmpCollector::dump_pdu(netsnmp_pdu *p) {
    printf("PDU I'm at: %p\n",p);
    printf("Version id: %ld\n",p->version);
    printf("Command id: %d\n",p->command);
    printf("Req id: %ld\n",p->reqid);
    printf("Message id: %ld\n",p->msgid);
    printf("Transaction id: %ld\n",p->transid);
    printf("Session id: %ld\n",p->sessid);
    printf("Error stat: %ld\n",p->errstat);
    printf("Error index: %ld\n",p->errindex);
    printf("Time: %lu\n",p->time);
    printf("Flags: %lu\n",p->flags);
    printf("Security model: %d\n",p->securityModel);
    printf("Security level: %d\n",p->securityLevel);
    printf("Msg parse: %d\n",p->msgParseModel);
    printf("Transport data: %p\n",p->transport_data);
    printf("Transport data length: %d\n",p->transport_data_length);
    printf("Domain: %p\n",p->tDomain);
    printf("Domain len: %ld\n",p->tDomainLen);
    printf("Variables: %p\n",p->variables);
    printf("Community: %p\n",p->community);
    printf("Community len: %ld\n",p->community_len);
    printf("Enterprise: %p\n",p->enterprise);
    printf("Enterprise length: %zu\n",p->enterprise_length);
    printf("Trap: %ld\n",p->trap_type);
    printf("Specific type: %ld\n",p->specific_type);
    printf("Agent address[0]: %u\n",p->agent_addr[0]);
    printf("Agent address[1]: %u\n",p->agent_addr[1]);
    printf("Agent address[2]: %u\n",p->agent_addr[2]);
    printf("Agent address[3]: %u\n",p->agent_addr[3]);
}

void snmpCollector::setRuntimeMetrics(RuntimeMetrics* metrics) {
    runtime_metrics_ = metrics;
}

void snmpCollector::setOIDs(string strOIDs) {
    pdu = snmp_pdu_create(SNMP_MSG_GET);

    oidList = splitString(strOIDs, ',');

    for (list<string>::const_iterator it = oidList.begin(); it != oidList.end(); ++it) {
        anOID_len = MAX_OID_LEN;
        if (NULL == snmp_parse_oid(it->c_str(), anOID, &anOID_len)) {
           throw invalidOIDParsing();
        }
        snmp_add_null_var(pdu, anOID, anOID_len);
    }
}

void snmpCollector::updateOIDs() {
    pdu = snmp_pdu_create(SNMP_MSG_GET);

    for (list<string>::const_iterator it = oidList.begin(); it != oidList.end(); ++it) {
        anOID_len = MAX_OID_LEN;
        if (NULL == snmp_parse_oid(it->c_str(), anOID, &anOID_len)) {
            throw invalidOIDParsing();
        }
        snmp_add_null_var(pdu, anOID, anOID_len);
    }
}

void snmpCollector::setLocation(string location){
    this->location = location;
}

vector<vardata> snmpCollector::collectData() {
    netsnmp_session *ss = NULL;
    vector<vardata> retValue;
    int status;

    updateCharPointers();

    if ( !(ss = snmp_open(&session)) ) {
        throw invalidSession();
    }

    updateOIDs();

    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_TIMEOUT) {
        throw snmpTimeout(hostname);
    } else if (status != STAT_SUCCESS) {
        throw dataCollectionError(hostname);
    }
    if (NULL != response) {
        retValue = packCollectedData(response);
        snmp_free_pdu(response);
    }
    snmp_close(ss);

    return retValue;
}

vector<vardata> snmpCollector::packCollectedData(netsnmp_pdu *response) {
    vector<vardata> retValue;

    if (response->errstat != SNMP_ERR_NOERROR) {
        dump_pdu(response);
        throw packetError();
    }

    for(netsnmp_variable_list *vars = response->variables; vars; vars = vars->next_variable) {
        vardata *var;
        string strData;
        struct in_addr addr;

        switch(vars->type) {
           case ASN_IPADDRESS:
                addr.s_addr = *vars->val.integer;
                strData = string( inet_ntoa(addr) );
                var = new vardata(strData);
                break;
           case ASN_OCTET_STR:
                strData = string( (char*) vars->val.string, vars->val_len);
                var = new vardata(strData);
                break;
            case ASN_INTEGER:
                var = new vardata(*vars->val.integer);
                break;
            case ASN_COUNTER:
            case ASN_GAUGE:
            case ASN_TIMETICKS:
                var = new vardata(*(uint32_t*)vars->val.integer);
                break;
            case ASN_OPAQUE_FLOAT:
                var = new vardata(*vars->val.floatVal);
                break;
            case ASN_OPAQUE_DOUBLE:
                var = new vardata(*vars->val.doubleVal);
                break;
            case ASN_OPAQUE_COUNTER64:
                var = new vardata(vars->val.counter64->high);
                break;
            default:
                var = new vardata(string("Unsupported data type"));
                break;
        }

        char buffer[STRING_BUFFER_SIZE];
        snprint_objid(buffer, STRING_BUFFER_SIZE, vars->name, vars->name_length);
        if(NULL != runtime_metrics_ && false == runtime_metrics_->IsTrackingLabel(static_cast<const char*>(buffer))) {
            runtime_metrics_->TrackSensorLabel(static_cast<const char*>(buffer));
        }
        if (NULL != var) {
            if(NULL == runtime_metrics_ ||
               runtime_metrics_->DoCollectMetrics(static_cast<const char*>(buffer))) {
                var->setKey(string(buffer));
                retValue.push_back(*var);
            }
            delete var;
        }
    }
    return retValue;
}

void snmpCollector::setPrivacyLevel(priv_protocol priv) {
    switch(priv){
        case NOPRIV:
            break;
        case DES:
            break;
        case AES:
            break;
    }
}

void snmpCollector::setSecurityLevel(sec_type sec) {
    switch(sec){
        case NOAUTH:
            session.securityLevel = SNMP_SEC_LEVEL_NOAUTH;
            break;
        case AUTHNOPRIV:
            session.securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
            break;
        case AUTHPRIV:
            session.securityLevel = SNMP_SEC_LEVEL_AUTHPRIV;
            break;
    }
}

void snmpCollector::setMD5Authentication(string password) {
    session.securityAuthProto = usmHMACMD5AuthProtocol;
    session.securityAuthProtoLen = sizeof(usmHMACMD5AuthProtocol)/sizeof(oid);
    (void) setAuthentication(password);
}

void snmpCollector::setSHA1Authentication(string password) {
    session.securityAuthProto = usmHMACSHA1AuthProtocol;
    session.securityAuthProtoLen = sizeof(usmHMACSHA1AuthProtocol)/sizeof(oid);
    (void) setAuthentication(password);
}

void snmpCollector::setAuthentication(string password) {
    session.securityAuthKeyLen = USM_AUTH_KU_LEN;
    if (generate_Ku(session.securityAuthProto,
                    session.securityAuthProtoLen,
                    (u_char *) password.c_str(), password.size(),
                    session.securityAuthKey,
                    &session.securityAuthKeyLen) != SNMPERR_SUCCESS) {
        throw invalidPassword();
    }
}

void snmpCollector::storeCharsAndLength(string s, char **c_str, size_t *len) {
    *c_str = const_cast<char*>(s.c_str());
    *len = s.length();
}

list<string> snmpCollector::splitString(string input, char delimiter) {
    string item;
    list<string> retValue;

    for (string::const_iterator it = input.begin(); it != input.end(); it++) {
        if (*it == delimiter && !item.empty()) {
            retValue.push_back(trim(item));
            item.clear();
        } else {
            item += *it;
        }
    }
    if (!item.empty())
         retValue.push_back(trim(item));
    return retValue;
}

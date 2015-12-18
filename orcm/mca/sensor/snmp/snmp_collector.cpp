/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_collector.h"

snmpCollector::snmpCollector() {
    snmpCollector("","");
}

snmpCollector::snmpCollector(string host, string user) {
    hostname = string(host);
    username = string(user);

    snmp_sess_init( &session );
    session.peername = const_cast<char*>(hostname.c_str());
    session.version = SNMP_VERSION_1;
    storeCharsAndLength(username, (char**) &session.community, &session.community_len);
}

snmpCollector::snmpCollector(string host, string user, string pass) {
    snmpCollector(host, user, pass, MD5);
}

snmpCollector::snmpCollector(string host, string user, string pass, auth_type auth) {
    snmpCollector(host, user, pass, auth, AUTHNOPRIV);
}

snmpCollector::snmpCollector(string host, string user, string pass, auth_type auth, sec_type sec) {
    hostname = string(host);
    username = string(user);
    password = string(pass);

    snmp_sess_init( &session );
    session.peername = const_cast<char*>(hostname.c_str());
    session.version=SNMP_VERSION_3;

    storeCharsAndLength(username, &session.securityName, &session.securityNameLen);
    setSecurityLevel(sec);

    switch(auth) {
        case MD5:
            setMD5Authentication(password);
            break;
        case SHA1:
            setSHA1Authentication(password);
            break;
    }
}

snmpCollector::~snmpCollector() {
}

void snmpCollector::dump_pdu(netsnmp_pdu *p) {
    printf("PDU I'm at: %p\n",p);
    printf("Version id: %d\n",p->version);
    printf("Command id: %d\n",p->command);
    printf("Req id: %d\n",p->reqid);
    printf("Message id: %d\n",p->msgid);
    printf("Transaction id: %d\n",p->transid);
    printf("Session id: %d\n",p->sessid);
    printf("Error stat: %d\n",p->errstat);
    printf("Error index: %d\n",p->errindex);
    printf("Time: %u\n",p->time);
    printf("Flags: %u\n",p->flags);
    printf("Security model: %d\n",p->securityModel);
    printf("Security level: %d\n",p->securityLevel);
    printf("Msg parse: %d\n",p->msgParseModel);
    printf("Transport data: %p\n",p->transport_data);
    printf("Transport data length: %d\n",p->transport_data_length);
    printf("Domain: %p\n",p->tDomain);
    printf("Domain len: %d\n",p->tDomainLen);
    printf("Variables: %p\n",p->variables);
    printf("Community: %p\n",p->community);
    printf("Community len: %d\n",p->community_len);
    printf("Enterprise: %p\n",p->enterprise);
    printf("Enterprise length: %d\n",p->enterprise_length);
    printf("Trap: %d\n",p->trap_type);
    printf("Specific type: %d\n",p->specific_type);
    printf("Agent address[0]: %u\n",p->agent_addr[0]);
    printf("Agent address[1]: %u\n",p->agent_addr[1]);
    printf("Agent address[2]: %u\n",p->agent_addr[2]);
    printf("Agent address[3]: %u\n",p->agent_addr[3]);
}

void snmpCollector::dump_session(netsnmp_session *s) {
    printf("Session I'm at: %p\n",s);
    printf("Version: %d\n",s->version);
    printf("Retries: %d\n",s->retries);
    printf("Timeout: %d\n",s->timeout);
    printf("Flags: %u\n",s->flags);
    printf("Subsession: %p\n",s->subsession);
    printf("Next: %p\n",s->next);
    printf("Peername: %s\n",s->peername);
    printf("Remote port: %u\n",s->local_port);
    printf("Localname: %s\n",s->localname);
    printf("Errno: %d\n", s->s_errno);
    printf("Library Errno: %d\n", s->s_snmp_errno);
    printf("Session id: %d\n", s->sessid);
}


void snmpCollector::setOIDs(string strOIDs) {
    pdu = snmp_pdu_create(SNMP_MSG_GET);

    oidList = splitString(strOIDs, ',');

    anOID_len = MAX_OID_LEN;
    for (list<string>::const_iterator it = oidList.begin(); it != oidList.end(); ++it) {
        if (1 != read_objid(it->c_str(), anOID, &anOID_len)) {
           throw invalidOIDParsing();
        }
        snmp_add_null_var(pdu, anOID, anOID_len);
    }
}

void snmpCollector::updateOIDs() {
    pdu = snmp_pdu_create(SNMP_MSG_GET);

    anOID_len = MAX_OID_LEN;
    for (list<string>::const_iterator it = oidList.begin(); it != oidList.end(); ++it) {
        if (1 != read_objid(it->c_str(), anOID, &anOID_len)) {
            throw invalidOIDParsing();
        }
        snmp_add_null_var(pdu, anOID, anOID_len);
    }
}

void snmpCollector::setLocation(string location){
    this->location = location;
}

list<string> snmpCollector::getRequestedOIDs() {
    list<string> retValue;

    if (NULL == pdu) {
        throw invalidOIDParsing();
    }
    for(netsnmp_variable_list* vars = pdu->variables; vars; vars = vars->next_variable) {
        char buffer[STRING_BUFFER_SIZE];
        snprint_objid(buffer, STRING_BUFFER_SIZE, vars->name, vars->name_length);
        retValue.push_back(string(buffer));
    }
    return retValue;
}

vector<vardata> snmpCollector::collectData() {
    netsnmp_session *ss = NULL;
    vector<vardata> retValue;
    int status;

    if ( !(ss = snmp_open(&session)) ) {
        throw invalidSession();
    }

    updateOIDs();

    status = snmp_synch_response(ss, pdu, &response);
    if (status == STAT_TIMEOUT) {
        throw snmpTimeout();
    } else if (status != STAT_SUCCESS) {
        throw dataCollectionError();
    }
    retValue = packCollectedData(response);
    if (response) {
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

        switch(vars->type) {
           case ASN_OCTET_STR:
                strData = string( (char*) vars->val.string, vars->val_len);
                var = new vardata(strData);
                break;
            case ASN_INTEGER:
                var = new vardata(*vars->val.integer);
                break;
            case ASN_OPAQUE_FLOAT:
                var = new vardata(*vars->val.floatVal);
                break;
            case ASN_OPAQUE_DOUBLE:
                var = new vardata(*vars->val.doubleVal);
                break;
            default:
                var = new vardata(string("Unsupported data type"));
                break;
        }

        char buffer[STRING_BUFFER_SIZE];
        snprint_objid(buffer, STRING_BUFFER_SIZE, vars->name, vars->name_length);
        var->setKey(string(buffer));

        retValue.push_back(*var);
        if (NULL != var) {
            delete var;
        }
    }

    return retValue;
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
    session.securityAuthKeyLen = USM_AUTH_KU_LEN;

    if (generate_Ku(session.securityAuthProto,
                    session.securityAuthProtoLen,
                    (u_char *) password.c_str(), password.size(),
                    session.securityAuthKey,
                    &session.securityAuthKeyLen) != SNMPERR_SUCCESS) {
        throw invalidPassword();
    }
}

void snmpCollector::setSHA1Authentication(string password) {
    setMD5Authentication(password); // Pending implementation
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
         retValue.push_back(item);
    return retValue;
}

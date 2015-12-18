/*
 * Copyright (c) 2015  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "snmp_parser.h"

/**
 * @brief Function that removes the white spaces from the beginning and the end
 *        of a string.
 *
 * @param s String from which you want to remove the white spaces.
 *
 * @return String without white spaces at the beginning and the end.
 */
static inline string &trim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), not1(ptr_fun<int, int>(isspace))));
    s.erase(find_if(s.rbegin(), s.rend(), not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}

/**
 * @brief Function that splits a string into several components using a delimiter.
 *
 * @param str String that you want to split into several elements.
 *
 * @param delimiter Pattern that indicates when an element ends and another begins.
 *
 * @param trim_tokens Flag that indicates if you want to remove the white spaces from each element or not.
 *
 * @return A vector with the splitted elements.
 */
static inline vector<string> splitString(string str, string delimiter=",", bool trim_tokens=true){
    vector<string> token_vector;
    size_t pos = 0;
    string token;

    while ((pos = str.find(delimiter)) != string::npos) {
        token = str.substr(0, pos);
        str.erase(0, pos + 1);
        if(trim_tokens)
            trim(token);
        token_vector.push_back(token);
    }

    if(trim_tokens)
        trim(str);
    token_vector.push_back(str);

    return token_vector;
}

/**
 * @brief Function that joins two vectors without duplicates.
 *
 * @param main_vector Vector where you want the join to be done.
 *
 * @param join_vector Vector that you want to integrate to the main_vector.
 *
 * @return A reference to the main_vector with the join done.
 */
static inline vector<string> &unique_str_vector_join(vector<string> &main_vector, vector<string> join_vector) {
    for (vector<string>::iterator it=join_vector.begin(); it != join_vector.end(); ++it){
        if (find(main_vector.begin(), main_vector.end(), *it) == main_vector.end())
            main_vector.push_back(*it);
    }

    return main_vector;
}

static inline snmpCollectorMap unique_map_join(snmpCollectorMap &main_map, snmpCollectorMap join_map) {
    for (snmpCollectorMap::iterator it=join_map.begin(); it != join_map.end(); ++it){
        if (main_map.find(it->first) == main_map.end())
            main_map.insert(*it);
    }
    return main_map;
}

static inline bool stringMatchRegex(string str, string pattern){
    regex_t regex_comp;
    regcomp(&regex_comp, pattern.c_str(), REG_ICASE|REG_EXTENDED);
    return !regexec(&regex_comp, str.c_str(), 0, NULL, 0);
}

snmpParser::snmpParser(string filePath) {
    SNMP_DEFAULT_FILE_PATH = string(opal_install_dirs.prefix) + string("/etc/snmp.conf");
    setParseFile(filePath);
}

snmpParser::~snmpParser(){
    unsetParseFile();
}

/**
 * @brief Function that sets the file to be parsed. If no file is provided the default will be set.
 *
 * @param filePath Path to the file (including the full file name) to be parsed.
 *
 */
void snmpParser::setParseFile(string filePath){
    unsetParseFile();
    if( ""==filePath){
        _filePath=SNMP_DEFAULT_FILE_PATH;
    } else
        _filePath=filePath;
}

/**
 * @brief Function that unsets the file to be parsed (if its loaded).
 *
 */
void snmpParser::unsetParseFile(){
    closeConfigFile();
    _filePath="";
}


/**
 * @brief Function that parses the SNMP configuration file and returns a vector
 *        with the SNMP objects to be monitored by this host.
 *
 * @return Vector with the SNMP devices (and its configuration) to be monitored by this host.
 */
vector<snmpCollector> snmpParser::parse() {
    vector<snmpCollector> snmp_vector;

    openConfigFile();
    snmp_vector = getSnmpCollectorVector();
    closeConfigFile();

    return snmp_vector;
}

void snmpParser::openConfigFile() {
    _snmp_file.open(_filePath.c_str());
    if( !_snmp_file )
        throw fileNotFound();
}

void snmpParser::closeConfigFile() {
    if( _snmp_file.is_open() )
        _snmp_file.close();
}

vector<snmpCollector> snmpParser::getSnmpCollectorVector() {
    vector<snmpCollector> snmp_collector_vector;
    vector<string> hostnames;
    snmpCollectorMap snmp_config_map;

    getHostnameVectorAndSnmpConfigMap(hostnames, snmp_config_map);

    for (vector<string>::iterator host=hostnames.begin(); host!=hostnames.end(); ++host){
        if (snmp_config_map.find(*host) != snmp_config_map.end())
            snmp_collector_vector.push_back(snmp_config_map[*host]);
    }

    return snmp_collector_vector;
}

void snmpParser::getHostnameVectorAndSnmpConfigMap(vector<string> &hostnames, snmpCollectorMap &snmp_config_map){
    string line="";

    while(!_snmp_file.eof()){
        getline(_snmp_file, line);
        if(!_snmp_file.eof() && isNodesTag(line))
            processNodeTag(hostnames);
        else if (!_snmp_file.eof() && isSnmpConfigTag(line))
            processSnmpConfigTag(snmp_config_map);
    }
}

void snmpParser::processNodeTag(vector<string> &hostnames){
    vector<string> tag_entries;
    tag_entries = getTagEntriesVector();
    unique_str_vector_join(hostnames, getMonitoredHostNamesFromTagEntries(tag_entries));
}

void snmpParser::processSnmpConfigTag(snmpCollectorMap &snmp_config_map){
    vector<string> tag_entries;
    tag_entries = getTagEntriesVector();
    unique_map_join(snmp_config_map, getSnmpConfigMapFromTagEntries(tag_entries));
}

vector<string> snmpParser::getTagEntriesVector(){
    string line="";
    vector<string> tag_entries;
    streampos last_line = _snmp_file.tellg();

    getline(_snmp_file, line);
    while(!_snmp_file.eof() && !isTag(line)){
        last_line = _snmp_file.tellg();
        tag_entries.push_back(line);
        getline(_snmp_file, line);
    }

    if (isTag(line))
       _snmp_file.seekg(last_line);

    return tag_entries;
}

vector<string> snmpParser::getMonitoredHostNamesFromTagEntries(vector<string> tag_entries){
    vector<string> viewed_host_names;

    for (vector<string>::iterator line=tag_entries.begin(); line!=tag_entries.end(); ++line){
        if (isThisHostname(*line))
            unique_str_vector_join(viewed_host_names, getVectorOfValuesFromLine(*line));
    }

    return viewed_host_names;
}

snmpCollectorMap snmpParser::getSnmpConfigMapFromTagEntries(vector<string> tag_entries){
    vector<string> hostnames;
    string version = "";
    string user = "";
    string pass = "";
    string auth = "";
    string sec = "";
    string oids = "";
    string location = "";
    for (vector<string>::iterator line=tag_entries.begin(); line!=tag_entries.end(); ++line){
        if (isHostname(*line)){
            unique_str_vector_join(hostnames, getVectorOfValuesFromLine(*line));
        }
        else if (isVersion(*line)){
            version = getValueFromLine(*line);
        }
        else if (isUser(*line)){
            user = getValueFromLine(*line);
        }
        else if (isPass(*line)){
            pass = getValueFromLine(*line);
        }
        else if (isAuthentication(*line)){
            auth = getValueFromLine(*line);
        }
        else if (isSecurity(*line)){
            sec = getValueFromLine(*line);
        }
        else if (isOIDs(*line)){
            oids = getValueFromLine(*line);
        }
        else if (isLocation(*line)){
            location = getValueFromLine(*line);
        }
    }

    snmpCollectorMap snmp_config_data;
    for (vector<string>::iterator host=hostnames.begin(); host!=hostnames.end(); ++host){
        if (!version.compare("1"))
            snmp_config_data[*host] = getSnmpCollectorVersion1(*host, user, oids, location);
        else if (!version.compare("3") || !version.compare("2"))
            snmp_config_data[*host] = getSnmpCollectorVersion3(*host, user, pass, auth, sec, oids, location);
    }

    return snmp_config_data;
}

string snmpParser::getValueFromLine(string line){
    vector<string> tmp = splitString(line, "=");
    return 2<=tmp.size() ? tmp[1] : "";
}

vector<string> snmpParser::getVectorOfValuesFromLine(string line){
    string raw_node_list = getValueFromLine(line);
    vector<string> vec = splitString(raw_node_list);
    vector<string> hosts;

    for (vector<string>::iterator it=vec.begin(); it!=vec.end(); ++it)
        unique_str_vector_join(hosts, expandLogicalGroup(*it));

    return hosts;
}

snmpCollector snmpParser::getSnmpCollectorVersion1(string hostname, string user, string oids, string location){
    snmpCollector collector(hostname, user);
    collector.setLocation(location);
    collector.setOIDs(oids);
    return collector;
}

snmpCollector snmpParser::getSnmpCollectorVersion3(string hostname, string user,
        string pass, string auth, string sec, string oids, string location){
    snmpCollector collector(hostname, user, pass, getAuthType(auth), getSecType(sec));
    collector.setLocation(location);
    collector.setOIDs(oids);
    return collector;
}

auth_type snmpParser::getAuthType(string auth){
    if (!auth.compare("SHA1"))
        return SHA1;
    return MD5;
}

sec_type snmpParser::getSecType(string sec){
    if (!sec.compare("NOAUTH"))
        return NOAUTH;
    else if (!sec.compare("AUTHPRIV"))
        return AUTHPRIV;
    return AUTHNOPRIV;
}


vector<string> snmpParser::expandLogicalGroup(string str){
    char **hosts = NULL;
    int count = 0;

    if (ORCM_SUCCESS != orcm_logical_group_parse_array_string(const_cast<char*>(str.c_str()), &hosts))
    {
        return vector<string>();
    }

    count = opal_argv_count(hosts);

    return vector<string>(hosts, hosts+count);
}

bool snmpParser::isThisHostname(string line){
    char c_str[1024];
    gethostname(c_str, 1023);

    string hostname(c_str);
    bool is_this_hostname = stringMatchRegex(line, "^[[:blank:]]*"+hostname+"[[:blank:]]*=(.*)$");
    bool is_localhost = stringMatchRegex(line, "^[[:blank:]]*localhost[[:blank:]]*=(.*)$");
    return is_this_hostname || is_localhost;
}

bool snmpParser::isHostname(string line){
    return stringMatchRegex(line, "^[[:blank:]]*hostname[[:blank:]]*=(.*)$");;
}

bool snmpParser::isVersion(string line){
    return stringMatchRegex(line, "^[[:blank:]]*version[[:blank:]]*=(.*)$");
}

bool snmpParser::isUser(string line){
    return stringMatchRegex(line, "^[[:blank:]]*user[[:blank:]]*=(.*)$");
}

bool snmpParser::isPass(string line){
    return stringMatchRegex(line, "^[[:blank:]]*pass[[:blank:]]*=(.*)$");
}

bool snmpParser::isAuthentication(string line){
    return stringMatchRegex(line, "^[[:blank:]]*auth[[:blank:]]*=(.*)$");
}

bool snmpParser::isSecurity(string line){
    return stringMatchRegex(line, "^[[:blank:]]*sec[[:blank:]]*=(.*)$");
}

bool snmpParser::isOIDs(string line){
    return stringMatchRegex(line, "^[[:blank:]]*oids[[:blank:]]*=(.*)$");
}

bool snmpParser::isLocation(string line){
    return stringMatchRegex(line, "^[[:blank:]]*location[[:blank:]]*=(.*)$");
}

bool snmpParser::isNodesTag(string line){
    return stringMatchRegex(line, "^[[:blank:]]*<[[:blank:]]*nodes[[:blank:]]*>[[:blank:]]*$");
}

bool snmpParser::isSnmpConfigTag(string line){
    return stringMatchRegex(line, "^[[:blank:]]*<[[:blank:]]*snmp\\-config[[:blank:]]*>[[:blank:]]*$");
}

bool snmpParser::isTag(string line){
    return stringMatchRegex(line, "^[[:blank:]]*<[^>]*>[[:blank:]]*$");
}


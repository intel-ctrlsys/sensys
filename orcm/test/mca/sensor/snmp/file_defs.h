#ifndef FILE_DEFS_H
#define FILE_DEFS_H

#define EXPECTED_COLLECTORS_BY_CONFIG 22

#define SNMP_DEFAULT_FILE_NAME "orcm-site.xml"

#define SNMP_DEFAULT_XML_FILE \
"<snmp> \n\
    <config name=\"router\" version=\"1\" user=\"public\" location=\"Melaque\"> \n\
        <hostname>10.219.80.3</hostname> \n\
        <aggregator>localhost</aggregator> \n\
        <oids>IF-MIB::ifDescr.1001,IF-MIB::ifDescr.1000319</oids> \n\
    </config> \n\
    <config name=\"printer\" version=\"1\" user=\"public\" location=\"Lab X\"> \n\
        <hostname>10.219.10.30</hostname> \n\
        <aggregator>localhost</aggregator> \n\
        <oids>.1.3.6.1.2.1.25.3.2.1.3.1,.1.3.6.1.2.1.25.3.2.1.3.2</oids> \n\
    </config> \n\
</snmp> \n"

#define BASE_XML_NAME "base.xml"
#define BASE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define AUTHPRIV_SEC_VALUE_XML_NAME "authpriv_sec_value.xml"
#define AUTHPRIV_SEC_VALUE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHPRIV\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define DEFAULT_AUTH_VALUE_XML_NAME "default_auth_value.xml"
#define DEFAULT_AUTH_VALUE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"invalid_value___this_will_use_the_default_md5\" sec=\"AUTHNOPRIV\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define DEFAULT_SEC_VALUE_XML_NAME "default_sec_value.xml"
#define DEFAULT_SEC_VALUE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"invalid_value___this_will_use_the_default_authnopriv\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define EMPTY_XML_NAME "empty.xml"
#define EMPTY_XML_FILE ""

#define NO_AGGREGATORS_XML_NAME "no_aggregators.xml"
#define NO_AGGREGATORS_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"NOAUTH\" priv=\"DES\"> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define NOAUTH_SEC_VALUE_XML_NAME "noauth_sec_value.xml"
#define NOAUTH_SEC_VALUE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"NOAUTH\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define NO_HOSTNAME_XML_NAME "no_hostname.xml"
#define NO_HOSTNAME_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"NOAUTH\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define NO_TAGS_XML_NAME "no_tags.xml"
#define NO_TAGS_XML_FILE \
"Notice how there are nosnmp tags.. \n\
This is intended for test purposes. \n\
\n\
This is \n\
only a \n\
test :) \n"

#define NO_VERSION_NUMBER_XML_NAME "no_version_number.xml"
#define NO_VERSION_NUMBER_XML_FILE \
"<snmp> \n\
    <config name=\"snmp2\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define SHA1_AUTH_VALUE_XML_NAME "sha1_auth_value.xml"
#define SHA1_AUTH_VALUE_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"SHA1\" sec=\"NOAUTH\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define SUCCESSFUL_XML_NAME "successful.xml"
#define SUCCESSFUL_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define WRONG_OIDS_V1_XML_NAME "wrong_oids_v1.xml"
#define WRONG_OIDS_V1_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"DES\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>this_is_a_wrong_oids</oids> \n\
    </config> \n\
</snmp> \n"

#define  WRONG_OIDS_V3_XML_NAME "wrong_oids_v3.xml"
#define  WRONG_OIDS_V3_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"DES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>this_is_a_wrong_oids</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define WRONG_SNMP_TAG_ENTRIES_XML_NAME "wrong_snmp_tag_entries.xml"
#define WRONG_SNMP_TAG_ENTRIES_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHPRIV\" priv=\"DES\"> \n\
        <invalid>this_is_an_invalid_entry</invalid> \n\
        <another_one>invalid</another_one> \n\
        <yet_another_one>not_valid</yet_another_one> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" invalid=\"this_is_an_invalid_entry\" another_one=\"invalid\" yet_another_one=\"not_valid\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],switch33</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp3\" version=\"1\" user=\"user\" another_one=\"invalid\"> \n\
        <invalid>this_is_an_invalid_entry</invalid> \n\
        <yet_another_one>not_valid</yet_another_one> \n\
    </config> \n\
 </snmp> \n"

#define WRONG_VERSION_NUMBER_01_XML_NAME "wrong_version_number_01.xml"
#define WRONG_VERSION_NUMBER_01_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" pass=\"pass\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"3\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp3\" version=\"3\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define WRONG_VERSION_NUMBER_02_XML_NAME "wrong_version_number_02.xml"
#define WRONG_VERSION_NUMBER_02_XML_FILE \
"<snmp> \n\
    <config name=\"snmp2\" version=\"-1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define DEFAULT_PRIV_XML_NAME "default_priv.xml"
#define DEFAULT_PRIV_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"Some Invalid Value\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define AES_PRIV_XML_NAME "aes_priv.xml"
#define AES_PRIV_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"AES\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#define NOPRIV_PRIV_XML_NAME "nopriv_priv.xml"
#define NOPRIV_PRIV_XML_FILE \
"<snmp> \n\
    <config name=\"snmp1\" version=\"3\" user=\"user\" pass=\"12345678\" auth=\"MD5\" sec=\"AUTHNOPRIV\" priv=\"NOPRIV\"> \n\
        <aggregator>localhost</aggregator> \n\
        <hostname>server[2:0-20],server21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
    <config name=\"snmp2\" version=\"1\" user=\"user\" location=\"X Lab\"> \n\
        <aggregator>node01</aggregator> \n\
        <hostname>switches[2:0-20],switch21</hostname> \n\
        <oids>1.3.6.1.4.1.343.1.1.3.1,1.3.6.1.4.1.343.1.1.3.4</oids> \n\
    </config> \n\
</snmp> \n"

#endif

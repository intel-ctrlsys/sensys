/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
*/
#include "snmp_test_files.h"

#define BAK_EXT ".bak"

testFiles testFilesObj;

int testFiles::writeStringToFile(const string& file, const string& str)
{
    ofstream outputFile(file.c_str());
    if (outputFile.is_open()){
        outputFile << str;
        outputFile.close();
        return ORCM_SUCCESS;
    }
    return ORCM_ERROR;
}

inline bool testFiles::fileExists(const string& file)
{
    return ! system((string("test -f ") + file).c_str());
}

int testFiles::createBakFile(const string& file, const string& bakFile, unsigned int permissions)
{
    int ret = ORCM_ERROR;
    string cmd = "mv " + file +  " " + bakFile;
    if(0 == (ret = system(cmd.c_str()))){
        stringstream ss;
        ss << "chmod " << permissions << " " << bakFile;
        return system(ss.str().c_str());
    }
    return ret;
}

int testFiles::backupFile(const string& file, unsigned int permissions)
{
    if (fileExists(file)){
        string bakFile = file + BAK_EXT;
        if (!fileExists(bakFile)){
            return createBakFile(file, bakFile, permissions);
        }
        cout << "Error: Cannot backup file. " << bakFile << "already exists." << endl;
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

void testFiles::setSnmpDefaultFilePath()
{
    if (0 == SNMP_DEFAULT_FILE_PATH.compare("")){
        string prefix = (NULL != opal_install_dirs.prefix) ? string(opal_install_dirs.prefix) + "/etc/" : "";
        SNMP_DEFAULT_FILE_PATH = prefix + SNMP_DEFAULT_FILE_NAME;
    }
}

int testFiles::writeDefaultSnmpConfigFile()
{
    int ret = ORCM_ERROR;
    setSnmpDefaultFilePath();
    if (0 == (ret = backupFile(SNMP_DEFAULT_FILE_PATH,SNMP_DEFAULT_FILE_PERMISSIONS))){
        return writeStringToFile(SNMP_DEFAULT_FILE_PATH, SNMP_DEFAULT_XML_FILE);
    } else {
        cout << "writeDefaultSnmpConfigFile failed with " << ret << endl;
    }
    return ORCM_ERROR;
}

int testFiles::restoreBackupFile(const string& file)
{
    string bakFile = file + BAK_EXT;
    if (fileExists(bakFile)){
        string cmd = "mv " + bakFile + " " + file;
        return system(cmd.c_str());
    }
    return ORCM_SUCCESS;
}

int testFiles::restoreDefaultSnmpConfigFile()
{
    int ret = ORCM_ERROR;
    setSnmpDefaultFilePath();
    if (0 == (ret = restoreBackupFile(SNMP_DEFAULT_FILE_PATH))){
        return ORCM_SUCCESS;
    } else {
        cout << "restoreDefaultSnmpConfigFile failed with " << ret << endl;
    }
    return ORCM_ERROR;
}

int testFiles::writeTestFiles()
{
    int ret = ORCM_SUCCESS;
    ret += writeStringToFile(AUTHPRIV_SEC_VALUE_XML_NAME, AUTHPRIV_SEC_VALUE_XML_FILE);
    ret += writeStringToFile(DEFAULT_AUTH_VALUE_XML_NAME, DEFAULT_AUTH_VALUE_XML_FILE);
    ret += writeStringToFile(DEFAULT_SEC_VALUE_XML_NAME, DEFAULT_SEC_VALUE_XML_FILE);
    ret += writeStringToFile(EMPTY_XML_NAME, EMPTY_XML_FILE);
    ret += writeStringToFile(NO_AGGREGATORS_XML_NAME, NO_AGGREGATORS_XML_FILE);
    ret += writeStringToFile(NOAUTH_SEC_VALUE_XML_NAME, NOAUTH_SEC_VALUE_XML_FILE);
    ret += writeStringToFile(NO_HOSTNAME_XML_NAME, NO_HOSTNAME_XML_FILE);
    ret += writeStringToFile(NO_TAGS_XML_NAME, NO_TAGS_XML_FILE);
    ret += writeStringToFile(NO_VERSION_NUMBER_XML_NAME, NO_VERSION_NUMBER_XML_FILE);
    ret += writeStringToFile(SHA1_AUTH_VALUE_XML_NAME, SHA1_AUTH_VALUE_XML_FILE);
    ret += writeStringToFile(SUCCESSFUL_XML_NAME, SUCCESSFUL_XML_FILE);
    ret += writeStringToFile(WRONG_OIDS_V1_XML_NAME, WRONG_OIDS_V1_XML_FILE);
    ret += writeStringToFile(WRONG_OIDS_V3_XML_NAME, WRONG_OIDS_V3_XML_FILE);
    ret += writeStringToFile(WRONG_SNMP_TAG_ENTRIES_XML_NAME, WRONG_SNMP_TAG_ENTRIES_XML_FILE);
    ret += writeStringToFile(WRONG_VERSION_NUMBER_01_XML_NAME, WRONG_VERSION_NUMBER_01_XML_FILE);
    ret += writeStringToFile(WRONG_VERSION_NUMBER_02_XML_NAME, WRONG_VERSION_NUMBER_02_XML_FILE);
    return ret;
}

int testFiles::removeFile(const string& file)
{
    return system((string("rm  ") + file).c_str());
}

int testFiles::removeTestFiles()
{
    int ret = ORCM_SUCCESS;
    ret += removeFile(AUTHPRIV_SEC_VALUE_XML_NAME);
    ret += removeFile(DEFAULT_AUTH_VALUE_XML_NAME);
    ret += removeFile(DEFAULT_SEC_VALUE_XML_NAME);
    ret += removeFile(EMPTY_XML_NAME);
    ret += removeFile(NO_AGGREGATORS_XML_NAME);
    ret += removeFile(NOAUTH_SEC_VALUE_XML_NAME);
    ret += removeFile(NO_HOSTNAME_XML_NAME);
    ret += removeFile(NO_TAGS_XML_NAME);
    ret += removeFile(NO_VERSION_NUMBER_XML_NAME);
    ret += removeFile(SHA1_AUTH_VALUE_XML_NAME);
    ret += removeFile(SUCCESSFUL_XML_NAME);
    ret += removeFile(WRONG_OIDS_V1_XML_NAME);
    ret += removeFile(WRONG_OIDS_V3_XML_NAME);
    ret += removeFile(WRONG_SNMP_TAG_ENTRIES_XML_NAME);
    ret += removeFile(WRONG_VERSION_NUMBER_01_XML_NAME);
    ret += removeFile(WRONG_VERSION_NUMBER_02_XML_NAME);
    return ret;
}

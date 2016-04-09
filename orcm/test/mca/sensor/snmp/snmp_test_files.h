/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
*/
#ifndef _SNMP_TEST_FILES_H
#define _SNMP_TEST_FILES_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "orcm/mca/sensor/snmp/snmp_parser.h"
#include "file_defs.h"

#define SNMP_DEFAULT_FILE_PERMISSIONS 600

using namespace std;

class testFiles {
    public:
        testFiles() { SNMP_DEFAULT_FILE_PATH=""; }
        ~testFiles() {};
        int restoreDefaultSnmpConfigFile();
        int writeDefaultSnmpConfigFile();
        int writeTestFiles();
        int removeTestFiles();

    private:
        string SNMP_DEFAULT_FILE_PATH;
        void setSnmpDefaultFilePath();
        int writeStringToFile(const string& file, const string& str);
        int backupFile(const string& file, unsigned int permissions);
        int restoreBackupFile(const string& file);
        inline bool fileExists(const string& file);
        int createBakFile(const string& file, const string& bakFile, unsigned int permissions);
        inline int removeFile(const string& file);
};

extern testFiles testFilesObj;

#endif

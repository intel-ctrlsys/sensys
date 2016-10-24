/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
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

using namespace std;

class testFiles {
    public:
        int writeDefaultSnmpConfigFile() { return writeStringToFile(SNMP_DEFAULT_FILE_NAME, SNMP_DEFAULT_XML_FILE); }
        int removeDefaultSnmpConfigFile() { return removeFile(SNMP_DEFAULT_FILE_NAME); }
        int writeTestFiles();
        int removeTestFiles();
        int writeInvalidSnmpConfigFile() { return writeStringToFile(NO_AGGREGATORS_XML_NAME,
                                                                    NO_AGGREGATORS_XML_FILE); }
        int removeInvalidSnmpConfigFile() { return removeFile(NO_AGGREGATORS_XML_NAME); }

    private:
        int writeStringToFile(const string& file, const string& str);
        int removeFile(const string& file);
};

extern testFiles testFilesObj;

#endif

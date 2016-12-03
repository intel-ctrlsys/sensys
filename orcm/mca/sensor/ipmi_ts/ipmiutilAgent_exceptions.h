/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIUTILAGENT_EXCEPTIONS_H
#define IPMIUTILAGENT_EXCEPTIONS_H

#include <string>
#include <stdexcept>

const std::string getErrorMessage(int rc);
const std::string getCompletionMessage(int cc);

class baseException : public std::runtime_error
{
public:
    enum exceptType {ERROR, COMPLETION};
    baseException(std::string base, int code, exceptType t);
};

class badConnectionParameters : public baseException
{
public:
    badConnectionParameters(int rc) : baseException("Unable to set connection parameters:", rc, ERROR) {}
};

class unableToCollectSensorList : public baseException
{
public:
    unableToCollectSensorList() : baseException("Unable to retrieve sensor list from BMC.", 1, ERROR) {}
};

class unableToCollectSensorReadings : public baseException
{
public:
    unableToCollectSensorReadings() : baseException("Unable to retrieve sensor readings from BMC.", 1, ERROR) {}
};

#endif // IPMIUTILAGENT_EXCEPTIONS_H

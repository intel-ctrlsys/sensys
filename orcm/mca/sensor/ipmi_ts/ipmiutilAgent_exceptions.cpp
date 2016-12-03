/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ipmiutilAgent_exceptions.h"
#include <map>

using namespace std;

static const map<int, string> errorMessagesMap();
static const map<int,string> completionMessagesMap();

static const map<int, string> errorMessagesMap()
{
    map <int, string> errorList;
    errorList[0] = "completed successfully";
    errorList[-1] = "error -1";
    errorList[-2] = "send to BMC failed";
    errorList[-3] = "receive from BMC failed";
    errorList[-4] = "cannot connect to BMC";
    errorList[-5] = "abort signal caught";
    errorList[-6] = "timeout occurred";
    errorList[-7] = "length greater than max";
    errorList[-8] = "invalid lan parameter";
    errorList[-9] = "request not supported";
    errorList[-10] = "receive too short";
    errorList[-11] = "error resolving hostname";
    errorList[-12] = "error during ping";
    errorList[-13] = "BMC only supports lan v1";
    errorList[-14] = "BMC only supports lan v2";
    errorList[-15] = "unknown error";
    errorList[-16] = "cannot open IPMI driver";
    errorList[-17] = "invalid parameter";
    errorList[-18] = "access not allowed";
    errorList[-19] = "session dropped by BMC";
    errorList[-20] = "cannot open file";
    errorList[-21] = "item not found";
    errorList[-22] = "usage or help requested";
    errorList[-23] = "bad format";
    errorList[-504] = "error getting msg from BMC";
    return errorList;
}

static const map<int,string> completionMessagesMap()
{
    map<int, string> ccList;
    ccList[0] = "Command completed successfully";
    ccList[128] = "Invalid Session Handle or Empty Buffer";
    ccList[129] = "Lost Arbitration";
    ccList[130] = "Bus Error";
    ccList[131] = "NAK on Write - busy";
    ccList[132] = "Truncated Read";
    ccList[192] = "Node Busy";
    ccList[193] = "Invalid Command";
    ccList[194] = "Command invalid for given LUN";
    ccList[195] = "Timeout while processing command";
    ccList[196] = "Out of space";
    ccList[197] = "Invalid Reservation ID, or cancelled";
    ccList[198] = "Request data truncated";
    ccList[199] = "Request data length invalid";
    ccList[200] = "Request data field length limit exceeded";
    ccList[201] = "Parameter out of range";
    ccList[202] = "Cannot return requested number of data bytes";
    ccList[203] = "Requested sensor, data, or record not present";
    ccList[204] = "Invalid data field in request";
    ccList[205] = "Command illegal for this sensor/record type";
    ccList[206] = "Command response could not be provided";
    ccList[207] = "Cannot execute duplicated request";
    ccList[208] = "SDR Repository in update mode, no response";
    ccList[209] = "Device in firmware update mode, no response";
    ccList[210] = "BMC initialization in progress, no response";
    ccList[211] = "Destination unavailable";
    ccList[212] = "Cannot execute command. Insufficient privilege level";
    ccList[213] = "Cannot execute command. Request parameters not supported";
    ccList[255] = "Unspecified error";
    return ccList;
}

const string getErrorMessage(int rc)
{
    static const map<int, string> errorList = errorMessagesMap();
    map<int, string>::const_iterator it = errorList.find(rc);
    return (it != errorList.end() ? it->second : "");
}

const string getCompletionMessage(int cc)
{
    static const map<int, string> ccList = completionMessagesMap();
    map<int, string>::const_iterator it = ccList.find(cc);
    return (it != ccList.end() ? it->second : "");
}

baseException::baseException(std::string base, int code, exceptType t) :
        std::runtime_error(base + (t == ERROR ? getErrorMessage(code) : getCompletionMessage(code)))
{
}

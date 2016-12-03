/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMIRESPONSE_H
#define IPMIRESPONSE_H

#include <vector>
#include <iostream>
typedef std::vector<unsigned char> buffer;

class ipmiResponse
{
private:
    bool success;
    buffer response;
    std::string errorMessage;
    std::string completionMessage;
    std::map<unsigned short int, std::string> sensorList;

    void setValues(std::string err, std::string completion, bool succ)
    {
        errorMessage = err;
        completionMessage = completion;
        success = succ;
    }
public:
    ipmiResponse()
    {
        success = false;
        errorMessage = "";
        completionMessage = "";
    }

    ipmiResponse(buffer *data, std::string err, std::string completion, bool succ)
    {
        if (NULL != data)
            response = *data;

        setValues(err, completion, succ);
    }

    ipmiResponse(unsigned char* ptr, int len, std::string err, std::string completion, bool succ)
    {
        response = buffer(ptr, ptr + len);
        setValues(err, completion, succ);
    }

    ipmiResponse(std::map<unsigned short int, std::string> list, std::string err, std::string completion, bool succ)
    {
        sensorList = list;
        setValues(err, completion, succ);
    }

    inline bool wasSuccessful() { return success; }
    inline std::string getErrorMessage() { return  errorMessage; }
    inline std::string getCompletionMessage() { return completionMessage; }
    inline std::map<unsigned short int, std::string> getSensorList() { return sensorList; };
    inline const buffer& getResponseBuffer() { return response; }
};

#endif //IPMIRESPONSE_H

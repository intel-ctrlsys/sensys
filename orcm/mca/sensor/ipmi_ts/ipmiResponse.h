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
#include "orcm/common/dataContainer.hpp"

typedef std::vector<unsigned char> buffer;

class ipmiResponse
{
private:
    bool success;
    buffer response;
    std::string errorMessage;
    std::string completionMessage;
    dataContainer readings;

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

    ipmiResponse(dataContainer data, std::string err, std::string completion, bool succ)
    {
        readings = data;
        setValues(err, completion, succ);
    }

    inline bool wasSuccessful() { return success; }
    inline std::string getErrorMessage() { return  errorMessage; }
    inline std::string getCompletionMessage() { return completionMessage; }
    inline const buffer& getResponseBuffer() { return response; }
    inline dataContainer getReadings() { return readings; };
};

#endif //IPMIRESPONSE_H

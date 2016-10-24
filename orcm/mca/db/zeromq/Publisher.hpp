//
// Copyright (c) 2016      Intel Corporation. All rights reserved.
// $COPYRIGHT$
//
// Additional copyrights may follow
//
// $HEADER$
//
#ifndef PUBLISHER_HPP
#define PUBLISHER_HPP

#include <string>

typedef void (*OuputCallbackFuncPtr)(int level, const char* message);

class Publisher
{
public:
    virtual ~Publisher() {};
    virtual void Init(int port, int threadsHint = 1, int maxBuffersHint = 10000, OuputCallbackFuncPtr callback = NULL) = 0;
    virtual void Finalize() = 0;
    virtual void PublishMessage(const std::string& key, const std::string& message) = 0;
    virtual void SetOutputCallback(OuputCallbackFuncPtr callback) = 0;
    virtual void Output(int level, const char* message) = 0;
};
#endif // PUBLISHER_HPP

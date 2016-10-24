/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef UDEXCEPTION_TESTS_H 
#define UDEXCEPTION_TESTS_H 
 
#include <string>
#include "gtest/gtest.h"

using std::string;

class exceptionTester : public testing::Test {
    protected:
        string label;
        static const size_t BLANK_SPACES = 2;
        const string probeMessage;
        string errorTypeLabel(const string& m);
        string messageLabel(const string& m);
        string fileLabel(const string& m);
        void checkLabelAndMessage(const string& m);
        void checkFilename(const string& m);

    public:
        exceptionTester() : probeMessage("Exception Message") {};
};

class criticalTest : public exceptionTester {
    public:
        criticalTest() : exceptionTester() { label = string("CRITICAL "); };
};

class warningTest : public exceptionTester {
    public:
        warningTest() : exceptionTester() { label = string("WARNING "); };
};

#endif

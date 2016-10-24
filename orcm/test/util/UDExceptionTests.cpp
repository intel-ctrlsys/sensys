/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <string>
#include <stdexcept>
#include "orcm/common/UDExceptions.h"

#include "UDExceptionTests.h"

using std::string;
using std::exception;

string exceptionTester::errorTypeLabel(const string& m) {
    size_t posSeparator = m.find(":");
    if (string::npos == posSeparator) {
        return string();
    }

    return m.substr(0, posSeparator);
}

string exceptionTester::messageLabel(const string& m) {
    size_t firstSeparator = m.find(":");
    if (string::npos == firstSeparator) {
        return string();
    }

    size_t secondSeparator = m.find(":", firstSeparator + 1);
    if (string::npos == secondSeparator) {
        return m.substr(firstSeparator + BLANK_SPACES);
    }

    size_t width = secondSeparator - firstSeparator - BLANK_SPACES - 1;

    return m.substr(firstSeparator + BLANK_SPACES, width);
}

string exceptionTester::fileLabel(const string& m) {
    size_t lastSeparator = m.find_last_of(":");
    return m.substr(lastSeparator + BLANK_SPACES); 
}

void exceptionTester::checkLabelAndMessage(const string& failureMessage) {
    EXPECT_EQ(0, errorTypeLabel(failureMessage).compare(label));
    EXPECT_EQ(0, messageLabel(failureMessage).compare(probeMessage));
}

void exceptionTester::checkFilename(const string& failureMessage) {
    EXPECT_EQ(0, fileLabel(failureMessage).compare(__FILE__));
}

TEST_F(criticalTest, simpleMessage) {
    using udlib::Critical;

    try {
        throw Critical(probeMessage); 
    } catch (Critical e) {
        checkLabelAndMessage(e.what());
    } catch (exception e) {
        FAIL();
    }
}

TEST_F(criticalTest, fileName) {
    using udlib::Critical;

    try {
        UDLIB_THROW(Critical, probeMessage);
    } catch (Critical e) {
        checkLabelAndMessage(e.what());
        checkFilename(e.what());
    } catch (exception e) {
        FAIL();
    }
}

TEST_F(warningTest, simpleMessage) {
    using udlib::Warning;

    try {
        throw Warning(probeMessage); 
    } catch (Warning e) {
        checkLabelAndMessage(e.what());
    } catch (exception e) {
        FAIL();
    }
}

TEST_F(warningTest, fileName) {
    using udlib::Warning;

    try {
        UDLIB_THROW(Warning, probeMessage);
    } catch (Warning e) {
        checkLabelAndMessage(e.what());
        checkFilename(e.what());
    } catch (exception e) {
        FAIL();
    }
}


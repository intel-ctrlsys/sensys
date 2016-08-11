/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef EXCEPTIONS_FOR_USER_DEFINED_INTERFACES_H
#define EXCEPTIONS_FOR_USER_DEFINED_INTERFACES_H

#include <stdexcept>

#define UDLIB_THROW(E,message) \
    throw E(message, __FILE__);

namespace udlib {
    using std::string;

    const static string criticalLabel("CRITICAL : ");
    const static string warningLabel("WARNING : ");

    class Critical : public std::runtime_error {
        public:
            Critical(const string &m) : std::runtime_error(criticalLabel + m) {};
            Critical(const string &m, const string &file) : std::runtime_error(
                 criticalLabel + m + string(" : ") + file) {}
    };

    class Warning : public std::runtime_error {
        public:
            Warning(const string &m) : std::runtime_error(warningLabel + m) {};
            Warning(const string &m, const string &file) : std::runtime_error(
                 warningLabel + m + string(" : ") + file) {}
    };
}

#endif

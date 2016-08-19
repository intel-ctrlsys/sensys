#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

#
# For make V=1 verbosity
#

include $(top_srcdir)/Makefile.ompi-rules

#
# Tests.  "make check" return values:
#
# 0:              pass
# 77:             skipped test
# 99:             hard error, stop testing
# other non-zero: fail
#

TESTS = test_util.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = util_tests

util_tests_SOURCES = \
    util_tests.cpp \
    util_tests.h \
    util_tests_mocking.cpp \
    util_tests_mocking.h \
    dataContainerTests.cpp \
    dataContainerTests.h \
    UDExceptionTests.cpp \
    sensorFactory_tests.cpp \
    sensorFactory_tests.h \
    dataContainerHelperTests.cpp \
    dataContainerHelperTests.h

MOCKING = \
    -Wl,--wrap=orte_errmgr_base_log \
    -Wl,--wrap=dlopen \
    -Wl,--wrap=dlsym \
    -Wl,--wrap=dlerror \
    -Wl,--wrap=dlclose \
    -Wl,--wrap=readdir

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lsensysplugins -lsensysplugins_helper -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

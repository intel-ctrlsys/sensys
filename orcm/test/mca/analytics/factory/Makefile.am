#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$

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

TESTS = analytics_factory_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = analytics_factory_tests

analytics_factory_tests_SOURCES = \
    analytics_factory_test.cpp \
    analytics_factory_test.h \
    analytics_factory_test_mocking.cpp \
    analytics_factory_test_mocking.h

MOCKING = \
    -Wl,--wrap=readdir \
    -Wl,--wrap=dlopen \
    -Wl,--wrap=dlerror \
    -Wl,--wrap=dlsym \
    -Wl,--wrap=dlclose

#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil -lsensysplugins $(MOCKING)

#
# Preprocessor flags
#
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

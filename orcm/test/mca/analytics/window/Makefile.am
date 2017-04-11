#
# Copyright (c) 2014-2017      Intel Corporation. All rights reserved.
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

TESTS = test_window.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = window_tests

window_tests_SOURCES = \
    analytics_window_test.cpp \
    analytics_window_test.h

WINDOW_BUILD_DIR=$(top_builddir)/orcm/mca/analytics/window

if MCA_BUILD_orcm_analytics_window_DSO

WINDOW_LIB=$(WINDOW_BUILD_DIR)/mca_analytics_window.la

else

WINDOW_LIB=$(WINDOW_BUILD_DIR)/libmca_analytics_window.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(WINDOW_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#

WINDOW_DIR=$(top_srcdir)/orcm/mca/analytics/window
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(WINDOW_DIR) -I$(ANALYTICS_UTIL_DIR)

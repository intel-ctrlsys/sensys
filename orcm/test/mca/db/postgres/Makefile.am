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

TESTS = postgres_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = postgres_tests

postgres_tests_SOURCES = \
    db_postgres_test.cpp \
    db_postgres_test.h

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util

#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(opal_db_postgres_LDFLAGS)

#
# Preprocessor flags
#
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) $(opal_db_postgres_CPPFLAGS) -I$(ANALYTICS_UTIL_DIR)

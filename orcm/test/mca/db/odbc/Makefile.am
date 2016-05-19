#
# Copyright (c) 2016      Intel, Inc. All rights reserved.
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

TESTS = odbc_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = odbc_tests

odbc_tests_SOURCES = \
    db_odbc_test.cpp \
    db_odbc_test.h

ODBC_BUILD_DIR=$(top_builddir)/orcm/mca/db/odbc

if MCA_BUILD_orcm_db_odbc_DSO

ODBC_LIB=$(ODBC_BUILD_DIR)/mca_db_odbc.la

else

ODBC_LIB=$(ODBC_BUILD_DIR)/libmca_db_odbc.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(ODBC_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil -lodbc

#
# Preprocessor flags
#

ODBC_DIR=$(top_srcdir)/orcm/mca/db/odbc
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(ODBC_DIR) -I$(ANALYTICS_UTIL_DIR)

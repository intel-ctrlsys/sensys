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

TESTS = genex_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = genex_tests

genex_tests_SOURCES = \
    analytics_genex_test.cpp \
    analytics_genex_test.h

GENEX_BUILD_DIR=$(top_builddir)/orcm/mca/analytics/genex

if MCA_BUILD_orcm_analytics_genex_DSO

GENEX_LIB=$(GENEX_BUILD_DIR)/mca_analytics_genex.la

else

GENEX_LIB=$(GENEX_BUILD_DIR)/libmca_analytics_genex.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(GENEX_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#

GENEX_DIR=$(top_srcdir)/orcm/mca/analytics/genex
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(GENEX_DIR) -I$(ANALYTICS_UTIL_DIR)

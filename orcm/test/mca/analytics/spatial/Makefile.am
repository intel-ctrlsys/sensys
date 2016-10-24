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

TESTS = spatial_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = spatial_tests

spatial_tests_SOURCES = \
    analytics_spatial_test.cpp \
    analytics_spatial_test.h

SPATIAL_BUILD_DIR=$(top_builddir)/orcm/mca/analytics/spatial

if MCA_BUILD_orcm_analytics_spatial_DSO

SPATIAL_LIB=$(SPATIAL_BUILD_DIR)/mca_analytics_spatial.la

else

SPATIAL_LIB=$(SPATIAL_BUILD_DIR)/libmca_analytics_spatial.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(SPATIAL_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#

SPATIAL_DIR=$(top_srcdir)/orcm/mca/analytics/spatial
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(SPATIAL_DIR) -I$(ANALYTICS_UTIL_DIR)

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

TESTS = extension_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = extension_tests

extension_tests_SOURCES = \
    analytics_extension_test.cpp \
    analytics_extension_test.h

EXTENSION_BUILD_DIR=$(top_builddir)/orcm/mca/analytics/extension

if MCA_BUILD_orcm_analytics_extension_DSO

EXTENSION_LIB=$(EXTENSION_BUILD_DIR)/mca_analytics_extension.la

else

EXTENSION_LIB=$(EXTENSION_BUILD_DIR)/libmca_analytics_extension.la

endif

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(EXTENSION_LIB) \
    $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil -lsensysplugins

#
# Preprocessor flags
#

EXTENSION_DIR=$(top_srcdir)/orcm/mca/analytics/extension
ANALYTICS_UTIL_DIR=$(top_srcdir)/orcm/test/mca/analytics/util
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(EXTENSION_DIR) -I$(ANALYTICS_UTIL_DIR)

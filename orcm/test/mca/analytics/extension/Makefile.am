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
    analytics_extension_test.hpp \
    analytics_extension_mocking.cpp \
    analytics_extension_mocking.h

MOCKING = \
    -Wl,--wrap=orcm_util_load_analytics_time_compute \
    -Wl,--wrap=orcm_analytics_base_log_to_database_event \
    -Wl,--wrap=orcm_util_load_orcm_value \
    -Wl,--wrap=malloc

ANALYTICS_UTIL_LIB_DIR=$(top_builddir)/orcm/test/mca/analytics/util
#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a \
        $(ANALYTICS_UTIL_LIB_DIR)/analytics_util.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

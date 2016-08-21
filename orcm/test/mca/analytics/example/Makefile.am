#
# Copyright (c) 2016     Intel, Inc. All rights reserved.
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

TESTS = analytics_example_test

#
# Executables to be built for "make check"
#

check_PROGRAMS = analytics_example_test

analytics_example_test_SOURCES = \
    analytics_example_test.cpp

#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(top_builddir)/orcm/mca/analytics/analytics_extension_example.la

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread

#
# Preprocessor flags
#
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)
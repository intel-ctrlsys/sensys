#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
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

TESTS = window_tests

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
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(WINDOW_LIB)

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#

WINDOW_DIR=$(top_srcdir)/orcm/mca/analytics/window
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(WINDOW_DIR)

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

TESTS = analytics_util

#
# Executables to be built for "make check"
#

check_PROGRAMS = analytics_util

analytics_util_SOURCES = \
    analytics_util.cpp \
    analytics_util.h

ORCM_UTIL_DIR=$(top_builddir)/orcm/util

ORCM_UTIL_LIB=$(ORCM_UTIL_DIR)/utils.lo
#
# Libraries we depend on
#

LDADD = \
    @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(ORCM_UTIL_LIB)

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil

#
# Preprocessor flags
#
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

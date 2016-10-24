#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

#
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

TESTS = test_ethtest.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = diag_ethtest

diag_ethtest_SOURCES = \
    ethtest_tests.h \
    ethtest_tests.cpp \
    ethtest_tests_mocking.cpp \
    ethtest_tests_mocking.h

MOCKING=\
    -Wl,--wrap=opal_dss_pack

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(top_builddir)/orcm/mca/diag/ethtest/diag_ethtest.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -ggdb $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

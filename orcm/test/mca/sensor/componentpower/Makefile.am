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

TESTS = componentpower_test.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = componentpower_tests

componentpower_tests_SOURCES = \
	componentpower_tests.cpp \
        componentpower_tests.h \
        componentpower_tests_mocking.cpp \
        componentpower_tests_mocking.h

MOCKING = \
    -Wl,--wrap=orte_errmgr_base_log \
    -Wl,--wrap=fopen\
    -Wl,--wrap=open \
    -Wl,--wrap=read \
    -Wl,--wrap=opal_progress_thread_init

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

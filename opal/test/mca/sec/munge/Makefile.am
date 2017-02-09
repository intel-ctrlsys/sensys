# Copyright (c) 2017      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

TESTS = munge_test.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = munge_tests

munge_tests_SOURCES = \
	munge_tests.cpp

# Mocking

MOCKING =

# Library dependencies

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal $(MOCKING)

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

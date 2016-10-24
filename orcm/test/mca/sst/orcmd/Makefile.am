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

TESTS = sst_orcmd_test.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = sst_orcmd_tests

sst_orcmd_tests_SOURCES = \
	sst_orcmd_test.cpp \
	sst_orcmd_mocking.cpp

MOCKING = \
	-Wl,--wrap=orte_ess_base_std_prolog \
	-Wl,--wrap=opal_pointer_array_init \
	-Wl,--wrap=orte_register_params \
	-Wl,--wrap=mca_base_framework_open \
	-Wl,--wrap=opal_dss_unpack

#
# Libraries we depend on
#

LDADD = \
	@GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

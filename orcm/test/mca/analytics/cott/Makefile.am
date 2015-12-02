#
# Copyright (c) 2015  Intel, Inc. All rights reserved.
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

TESTS = test_cott.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = cott_tests

cott_tests_SOURCES = \
	analyze_counter_tests.cpp \
	analyze_counter_tests.h \
	cott_mocking.cpp \
	cott_mocking.h

MOCKING = \
	-Wl,--wrap=malloc \
	-Wl,--wrap=opal_hash_table_init \
	-Wl,--wrap=opal_event_set \
	-Wl,--wrap=opal_event_set \
	-Wl,--wrap=orcm_analytics_base_activate_analytics_workflow_step

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

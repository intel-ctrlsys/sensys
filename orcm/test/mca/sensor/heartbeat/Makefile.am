#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
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

TESTS = test_heartbeat.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = heartbeat_tests

heartbeat_tests_SOURCES = \
    heartbeat_tests.cpp \
    heartbeat_tests.h \
    heartbeat_tests_mocking.cpp \
    heartbeat_tests_mocking.h

MOCKING=\
    -Wl,--wrap=orte_errmgr_base_log \
    -Wl,--wrap=orte_util_print_name_args \
    -Wl,--wrap=orte_get_job_data_object \
    -Wl,--wrap=opal_progress_thread_init \
    -Wl,--wrap=opal_progress_thread_finalize

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

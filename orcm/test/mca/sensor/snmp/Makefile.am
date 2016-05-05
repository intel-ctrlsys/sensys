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

TESTS = test_snmp.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = snmp_tests

snmp_tests_SOURCES = \
    vardata_tests.cpp \
    vardata_tests.h \
    snmp_collector_tests.cpp \
    snmp_collector_tests.h \
    snmp_tests_mocking.cpp \
    snmp_tests_mocking.h \
    snmp_parser_tests.cpp \
    snmp_parser_tests.h \
    snmp_test_files.cpp \
    snmp_test_files.h \
    file_defs.h

MOCKING=\
    -Wl,--wrap=orte_errmgr_base_log \
    -Wl,--wrap=opal_output_verbose \
    -Wl,--wrap=orte_util_print_name_args \
    -Wl,--wrap=orcm_analytics_base_send_data \
    -Wl,--wrap=opal_progress_thread_init \
    -Wl,--wrap=opal_progress_thread_finalize \
    -Wl,--wrap=snmp_open \
    -Wl,--wrap=snmp_synch_response \
    -Wl,--wrap=snmp_free_pdu \
    -Wl,--wrap=snmp_pdu_create \
    -Wl,--wrap=snmp_parse_oid \
    -Wl,--wrap=snmp_add_null_var \
    -Wl,--wrap=snprint_objid \
    -Wl,--wrap=orcm_util_load_orcm_analytics_value

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lnetsnmp -g $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

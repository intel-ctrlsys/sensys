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
snmp_testsdir = $(top_builddir)/orcm/test/mca/sensor/snmp/test_files
snmp_tests_DATA = test_files/authpriv_sec_value.conf \
            test_files/base.conf \
            test_files/default_auth_value.conf \
            test_files/default_sec_value.conf \
            test_files/empty.conf \
            test_files/multiple_hosts_on_multiple_snmp_config_tag.conf \
            test_files/multiple_hosts_on_snmp_config_tag.conf \
            test_files/noauth_sec_value.conf \
            test_files/no_node_information_for_this_host.conf \
            test_files/no_nodes.conf \
            test_files/no_snmp_config.conf \
            test_files/no_snmp_config_for_monitored_nodes.conf \
            test_files/no_tags.conf \
            test_files/no_version_number.conf \
            test_files/sha1_auth_value.conf \
            test_files/snmp.conf \
            test_files/successful.conf \
            test_files/wrong_oids_v1.conf \
            test_files/wrong_oids_v3.conf \
            test_files/wrong_snmp_tag_entries.conf \
            test_files/wrong_version_number_01.conf \
            test_files/wrong_version_number_02.conf

snmp_tests_SOURCES = \
    vardata_tests.cpp \
    vardata_tests.h \
    snmp_collector_tests.cpp \
    snmp_collector_tests.h \
    snmp_tests_mocking.cpp \
    snmp_tests_mocking.h \
    snmp_parser_tests.cpp \
    snmp_parser_tests.h

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
    -Wl,--wrap=read_objid \
    -Wl,--wrap=snmp_add_null_var \
    -Wl,--wrap=snprint_objid

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lnetsnmp -g $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

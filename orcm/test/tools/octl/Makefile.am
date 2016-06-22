#
# Copyright (c) 2015-2016  Intel, Inc. All rights reserved.
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

TESTS = test_octl.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = octl_tests

octl_tests_SOURCES = \
    octl_tests.cpp \
    octl_query_tests.cpp \
    octl_mocking.cpp \
    octl_led_tests.cpp \
    workflow_tests.cpp \
    octl_diag_tests.cpp \
    octl_resource_tests.cpp \
    octl_output_handler_tests.cpp

MOCKING = \
    -Wl,--wrap=orcm_cfgi_base_get_hostname_proc \
    -Wl,--wrap=get_bmc_info \
    -Wl,--wrap=opal_dss_pack \
    -Wl,--wrap=mca_base_framework_open \
    -Wl,--wrap=orcm_parser_base_select \
    -Wl,--wrap=orcm_logical_group_parse_array_string \
    -Wl,--wrap=opal_pointer_array_get_item

#
# Libraries we depend on
#

LDADD = \
        @GTEST_LIBRARY_DIR@/libgtest_main.a \
    $(top_builddir)/orcm/tools/octl/sensor.o \
    $(top_builddir)/orcm/tools/octl/query.o \
    $(top_builddir)/orcm/tools/octl/notifier.o \
    $(top_builddir)/orcm/tools/octl/workflow.o \
    $(top_builddir)/orcm/tools/octl/chassis-id.o \
    $(top_builddir)/orcm/tools/octl/smtp.o \
    $(top_builddir)/orcm/tools/octl/output_handler.o \
    $(top_builddir)/orcm/tools/octl/diag.o \
    $(top_builddir)/orcm/tools/octl/resource.o

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

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

TESTS = udsensors_test.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = udsensors_tests

udsensors_tests_SOURCES = \
    udsensors_tests.cpp

MOCKING = \
    -Wl,--wrap=opal_progress_thread_init \
    -Wl,--wrap=dlopen \
    -Wl,--wrap=dlsym \
    -Wl,--wrap=dlerror \
    -Wl,--wrap=dlclose \
    -Wl,--wrap=readdir \
    -Wl,--wrap=orcm_sensor_base_runtime_metrics_do_collect \
    -Wl,--wrap=opal_dss_pack \
    -Wl,--wrap=opal_dss_unpack \
    -Wl,--wrap=orcm_analytics_base_send_data \
    -Wl,--wrap=_ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv

#
# Libraries we depend on


LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lsensysplugins -lsensysplugins_helper -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

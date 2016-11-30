#
# Copyright (c) 2015  Intel Corporation. All rights reserved.
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

TESTS = ipmi_ts_tests.py

#
# Executables to be built for "make check"
#

check_PROGRAMS = ipmi_ts_tests

ipmi_ts_tests_SOURCES = \
    ipmi_ts_tests.h \
    ipmi_ts_tests.cpp

MOCKING = \
    -Wl,--wrap=opal_progress_thread_init \
    -Wl,--wrap=readdir \
    -Wl,--wrap=orcm_sensor_base_runtime_metrics_do_collect \
    -Wl,--wrap=dlopen \
    -Wl,--wrap=dlsym \
    -Wl,--wrap=dlerror \
    -Wl,--wrap=dlclose \
    -Wl,--wrap=opal_dss_pack \
    -Wl,--wrap=opal_dss_unpack \
    -Wl,--wrap=orcm_analytics_base_send_data \
    -Wl,--wrap=_ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv \
    -Wl,--wrap=_ZN19dataContainerHelper14deserializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv


#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto  $(MOCKING) # -lipmiutil

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

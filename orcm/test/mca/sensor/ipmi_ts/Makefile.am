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
    ipmi_ts_tests.cpp \
    ipmiHAL_tests.h \
    ipmiHAL_tests.cpp \
    ipmiutilAgent_tests.h \
    ipmiutilAgent_tests.cpp \
    ipmiutilAgent_mocks.h \
    ipmiutilAgent_mocks.cpp \
    ipmiResponseFactory_tests.hpp \
    ipmiResponseFactory_tests.cpp \
    ipmi_sensor_tests.hpp \
    ipmi_sensor_tests.cpp \
    timeoutHelper.hpp \
    timeoutHelper.cpp \
    persist_sel_record_id_tests.cpp \
    persist_sel_record_id_tests.h \
    sensor_ipmi_sel_mocked_functions.cpp \
    sensor_ipmi_sel_mocked_functions.h \
    ipmi_sel_collector_tests.h \
    ipmi_sel_collector_tests.cpp

MOCKING = \
    -Wl,--wrap=opal_progress_thread_init \
    -Wl,--wrap=orcm_sensor_base_runtime_metrics_do_collect \
    -Wl,--wrap=opal_dss_pack \
    -Wl,--wrap=opal_dss_unpack \
    -Wl,--wrap=orcm_analytics_base_send_data \
    -Wl,--wrap=_ZN19dataContainerHelper12serializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv \
    -Wl,--wrap=_ZN19dataContainerHelper14deserializeMapERSt3mapISs13dataContainerSt4lessISsESaISt4pairIKSsS1_EEEPv \
    -Wl,--wrap=_ZN14IpmiTestSensor8finalizeEv \
    -Wl,--wrap=_ZN14IpmiTestSensor4initEv \
    -Wl,--wrap=_ZN10ipmiSensor4initEv \
    -Wl,--wrap=_ZN10ipmiSensor8finalizeEv \
    -Wl,--wrap=set_lan_options \
    -Wl,--wrap=ipmi_cmd_mc \
    -Wl,--wrap=ipmi_cmd \
    -Wl,--wrap=ipmi_cmdraw \
    -Wl,--wrap=get_sdr_cache \
    -Wl,--wrap=find_sdr_next \
    -Wl,--wrap=rename \
    -Wl,--wrap=opal_output_verbose \
    -Wl,--wrap=localtime \
    -Wl,--wrap=strdup \
    -Wl,--wrap=opal_libevent2022_event_new
#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil $(MOCKING)
#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

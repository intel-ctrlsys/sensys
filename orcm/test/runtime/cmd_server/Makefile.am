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

TESTS = test_cmd_server.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = cmd_server_tests

cmd_server_tests_SOURCES = \
    cmd_server_tests.cpp \
    cmd_server_tests.h \
    cmd_server_tests_mocking.cpp \
    cmd_server_tests_mocking.h

MOCKING=\
    -Wl,--wrap=orte_notifier_base_set_config \
    -Wl,--wrap=get_notifier_policy \
    -Wl,--wrap=orte_notifier_base_get_config \
    -Wl,--wrap=load_ipmi_config_file \
    -Wl,--wrap=get_bmc_info \
    -Wl,--wrap=get_chassis_id_state \
    -Wl,--wrap=disable_chassis_id \
    -Wl,--wrap=enable_chassis_id \
    -Wl,--wrap=enable_chassis_id_with_timeout \
    -Wl,--wrap=orcm_util_append_orcm_value

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

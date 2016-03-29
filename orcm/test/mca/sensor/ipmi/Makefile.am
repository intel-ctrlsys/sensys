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

TESTS = ipmi_tests

#
# Executables to be built for "make check"
#

check_PROGRAMS = ipmi_tests

ipmi_tests_SOURCES = \
	ipmi_sel_collector_tests.cpp \
	ipmi_sel_collector_tests.h \
	persist_sel_record_id_tests.cpp \
	persist_sel_record_id_tests.h \
	sel_data.h \
	sensor_ipmi_sel_mocked_functions.cpp \
	sensor_ipmi_sel_mocked_functions.h \
	sensor_ipmi_tests.cpp \
	sensor_ipmi_tests.h \
	ipmi_collector_tests.cpp \
	ipmi_collector_tests.h \
	ipmi_parser_tests.cpp \
	ipmi_parser_tests.h

MOCKING=\
	-Wl,--wrap=rename \
	-Wl,--wrap=set_lan_options \
	-Wl,--wrap=ipmi_cmd \
	-Wl,--wrap=ipmi_close \
	-Wl,--wrap=opal_output_verbose \
	-Wl,--wrap=getlogin_r \
	-Wl,--wrap=geteuid

#
# Libraries we depend on
#

LDADD = @GTEST_LIBRARY_DIR@/libgtest_main.a

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread -lcrypto -lipmiutil $(MOCKING)

#
# Preprocessor flags
#

AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir)

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

TESTS = cfgi_file10_test.py

TEST_EXTENSIONS = .py
PY_LOG_COMPILER = /usr/bin/python

#
# Executables to be built for "make check"
#

check_PROGRAMS = cfgi_file10_tests

cfgi_file10_tests_SOURCES = \
                         cfgi_file10_tests.cpp \
                         cfgi_file10_mocking.cpp

MOCKING = \
          -Wl,--wrap=__strdup \
          -Wl,--wrap=orcm_parser_base_open_file \
          -Wl,--wrap=orcm_parser_base_retrieve_document \
          -Wl,--wrap=orcm_parser_base_close_file           

CFGI_FILE10_BUILD_DIR=$(top_builddir)/orcm/mca/cfgi/file10

if MCA_BUILD_orcm_cfgi_file10_DSO

CFGI_FILE10_LIB=$(CFGI_FILE10_BUILD_DIR)/mca_cfgi_file10.la

else

CFGI_FILE10_LIB=$(CFGI_FILE10_BUILD_DIR)/libmca_cfgi_file10.la

endif          

#
# Libraries we depend on
#

LDADD = \
        @GTEST_LIBRARY_DIR@/libgtest_main.a \
        $(CFGI_FILE10_LIB) 

AM_LDFLAGS = -lorcm -lorcmopen-pal -lpthread $(MOCKING)

#
# Preprocessor flags
#

CFGI_FILE10_DIR=$(top_srcdir)/orcm/mca/cfgi/file10
AM_CPPFLAGS=-I@GTEST_INCLUDE_DIR@ -I$(top_srcdir) -I$(CFGI_FILE10_DIR)
